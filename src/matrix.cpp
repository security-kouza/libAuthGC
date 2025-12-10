#include <ATLab/matrix.hpp>

#include <cassert>
#include <sstream>
#include <immintrin.h>

#include <boost/dynamic_bitset.hpp>

#include <authed_bit.hpp>

namespace ATLab {
    namespace {
        using MatrixBlock = Matrix<bool>::Block64;
        constexpr size_t BitsPerBlock {Matrix<bool>::bits_per_block};

        std::vector<MatrixBlock> bitset_to_blocks(const Bitset& bits, const size_t blockCount) {
            const size_t requiredBlocks {calc_bitset_blockSize(bits.size())};
#ifdef DEBUG
            if (blockCount < requiredBlocks) {
                std::ostringstream sout;
                sout << "Bitset requires " << requiredBlocks << " blocks but only "
                    << blockCount << " provided.\n";
                throw std::invalid_argument{sout.str()};
            }
#endif // DEBUG
            std::vector<MatrixBlock> blocks(blockCount, 0);
            if (requiredBlocks) {
                boost::to_block_range(bits, blocks.begin());
            }
            return blocks;
        }

        bool row_parity(
            const MatrixBlock* rowBlocks,
            const std::vector<MatrixBlock>& bitBlocks,
            const size_t blockCount)
        {
            bool parity {false};
            for (size_t i {0}; i < blockCount; ++i) {
                const MatrixBlock mask {rowBlocks[i] & bitBlocks[i]};
                parity ^= static_cast<bool>(__builtin_popcountll(mask) & 1);
            }
            return parity;
        }

        [[nodiscard]]
        bool parity(const uint64_t* const data, const std::size_t count) noexcept {
            __m256i acc{_mm256_setzero_si256()};

            size_t i {0};
            for (; i + 4 <= count; i += 4) {
                const __m256i loaded {
                    _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i))
                };

                acc = _mm256_xor_si256(acc, loaded);
            }

            const __m128i low128 {_mm256_castsi256_si128(acc)};
            const __m128i high128 {_mm256_extracti128_si256(acc, 1)};

            const __m128i folded128 {_mm_xor_si128(low128, high128)};
            const __m128i folded64_vec {
                _mm_xor_si128(folded128, _mm_srli_si128(folded128, 8))
            };

            auto folded64 {static_cast<uint64_t>(_mm_cvtsi128_si64(folded64_vec))};

            // Remaining
            for (; i < count; ++i) {
                folded64 ^= data[i];
            }

            return _mm_popcnt_u64(folded64) & 1;
        }

        [[nodiscard]]
        bool is_zero(const uint64_t* const data, const std::size_t count) noexcept {
            std::size_t i{0};

            for (; i + 4 <= count; i += 4) {
                const __m256i vec{
                    _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i))
                };

                if (_mm256_testz_si256(vec, vec) == 0) {
                    return false;
                }
            }

            for (; i < count; ++i) {
                if (data[i] != 0) {
                    return false;
                }
            }

            return true;
        }
    }

    Bitset operator*(const Matrix<bool>& matrix, const Bitset& bits) {
#ifdef DEBUG
        if (matrix.colSize != bits.size()) {
            std::ostringstream sout;
            sout << "Sizes mismatch: "
                << "matrix(" << matrix.rowSize << ", " << matrix.colSize << "), bits(" << bits.size() << ").\n";
            throw std::invalid_argument{sout.str()};
        }
#endif // DEBUG
        Bitset res(matrix.rowSize, 0);
        const size_t blockCount {matrix.blocks_per_row()};
        const auto bitBlocks {bitset_to_blocks(bits, blockCount)};

        for (size_t rowIter {0}; rowIter != matrix.rowSize; ++rowIter) {
            bool parity {false};
            if (blockCount) {
                const MatrixBlock* rowBlocks {matrix.row_data(rowIter)};
                parity = row_parity(rowBlocks, bitBlocks, blockCount);
            }
            res[rowIter] = parity;
        }
        return res;
    }

    ITMacBits operator*(const Matrix<bool>& matrix, const ITMacBits& authedBits) {
#ifdef DEBUG
        if (authedBits.global_key_size() != 1) {
            std::ostringstream sout;
            sout << "authedBits has " << authedBits.global_key_size() << " global keys. Only accepting 1.\n";
            throw std::invalid_argument{sout.str()};
        }
        if (matrix.colSize != authedBits._bits.size()) {
            std::ostringstream sout;
            sout << "Sizes mismatch: matrix columns " << matrix.colSize
                << ", authedBits " << authedBits._bits.size() << "\n";
            throw std::invalid_argument{sout.str()};
        }
#endif // DEBUG
        Bitset bits(matrix.rowSize, 0);
        std::vector<emp::block> macs(matrix.rowSize);
        const size_t blockCount {matrix.blocks_per_row()};
        const auto bitBlocks {bitset_to_blocks(authedBits._bits, blockCount)};

        for (size_t row {0}; row != matrix.rowSize; ++row) {
            const auto rowView {matrix.row(row)};
            bits[row] = rowView.bitwise_inner_product(bitBlocks);
            macs[row] = rowView * authedBits._macs;
        }
        return {std::move(bits), std::move(macs)};
    }

    ITMacBitKeys operator*(const Matrix<bool>& matrix, const ITMacBitKeys& keys) {
        std::vector<emp::block> newLocalKeys;
        newLocalKeys.reserve(matrix.rowSize);

        for (size_t row {0}; row != matrix.rowSize; ++row) {
            const auto rowView {matrix.row(row)};
            newLocalKeys.push_back(rowView * keys._localKeys);
        }
        return {std::move(newLocalKeys), keys._globalKeys};
    }

    ITMacBlocks operator*(const Matrix<bool>& matrix, const ITMacBlocks& blocks) {
        constexpr size_t GLOBAL_KEY_SIZE {1}; // Only allowing one global key
#ifdef DEBUG
        if (blocks.global_key_size() != GLOBAL_KEY_SIZE) {
            std::ostringstream sout;
            sout << "blocks has " << blocks.global_key_size() << " global keys. Only accepting 1.\n";
            throw std::invalid_argument{sout.str()};
        }
        if (matrix.colSize != blocks.size()) {
            std::ostringstream sout;
            sout << "Sizes mismatch: matrix columns " << matrix.colSize
                << ", blocks " << blocks.size() << "\n";
            throw std::invalid_argument{sout.str()};
        }
#endif // DEBUG
        std::vector<emp::block> newBlocks, newMacs;
        newBlocks.reserve(matrix.rowSize);
        newMacs.reserve(matrix.rowSize);
        for (size_t row {0}; row != matrix.rowSize; ++row) {
            const auto rowView {matrix.row(row)};
             newBlocks.emplace_back(rowView * blocks._blocks);
             newMacs.emplace_back(rowView * blocks._macs);
        }
        return {std::move(newBlocks), std::move(newMacs), GLOBAL_KEY_SIZE};
    }

    ITMacBlockKeys operator*(const Matrix<bool>& matrix, const ITMacBlockKeys& keys) {
        constexpr size_t GLOBAL_KEY_SIZE {1}; // Only allowing one global key
#ifdef DEBUG
        if (keys.global_key_size() != GLOBAL_KEY_SIZE) {
            std::ostringstream sout;
            sout << "`keys` has " << keys.global_key_size() << " global keys. Only accepting 1.\n";
            throw std::invalid_argument{sout.str()};
        }
        if (matrix.colSize != keys.size()) {
            std::ostringstream sout;
            sout << "Sizes mismatch: matrix columns " << matrix.colSize
                << ", `keys` " << keys.size() << "\n";
            throw std::invalid_argument{sout.str()};
        }
#endif // DEBUG
        std::vector<emp::block> newLocalKeys;
        newLocalKeys.reserve(matrix.rowSize);
        for (size_t row {0}; row != matrix.rowSize; ++row) {
            const auto rowView {matrix.row(row)};
            newLocalKeys.emplace_back(rowView * keys._localKeys);
        }
        return {std::move(newLocalKeys), keys._globalKeys.front()};
    }

    bool Matrix<bool>::RowView::bitwise_inner_product(const std::vector<Block64>& bitBlocks) const {
        if (!_blockCount || !_blocks) {
            return false;
        }
#ifdef DEBUG
        if (bitBlocks.size() < _blockCount) {
            std::ostringstream sout;
            sout << "RowView expects at least " << _blockCount
                << " parity blocks but received " << bitBlocks.size() << ".\n";
            throw std::invalid_argument{sout.str()};
        }
#endif // DEBUG
        bool parityBit {false};
        for (size_t i {0}; i < _blockCount; ++i) {
            const Block64 mask {_blocks[i] & bitBlocks[i]};
            parityBit ^= static_cast<bool>(__builtin_popcountll(mask) & 1);
        }
        return parityBit;
    }

    bool Matrix<bool>::RowView::parity() const noexcept {
        return ATLab::parity(_blocks, _blockCount);
    }

    bool Matrix<bool>::RowView::empty() const noexcept {
        return is_zero(_blocks, _blockCount);
    }
}

#include <ATLab/matrix.hpp>

#include <cassert>
#include <sstream>

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

        class RowView {
            const MatrixBlock* _blocks;
            const size_t _blockCount;
            const size_t _colSize;
        public:
            RowView(const MatrixBlock* blocks, const size_t blockCount, const size_t colSize):
                _blocks(blocks),
                _blockCount(blockCount),
                _colSize(colSize)
            {}

            template<class Func>
            void for_each_set_bit(Func&& fn) const {
                if (!_blockCount) {
                    return;
                }
                for (size_t blockIter {0}; blockIter < _blockCount; ++blockIter) {
                    MatrixBlock mask {_blocks[blockIter]};
                    while (mask) {
                        const size_t bitOffset {
                            static_cast<size_t>(__builtin_ctzll(mask))
                        };
                        mask &= (mask - 1);
                        const size_t col {blockIter * BitsPerBlock + bitOffset};
                        if (col >= _colSize) {
                            break;
                        }
                        fn(col);
                    }
                }
            }
        };

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
            const MatrixBlock* rowBlocks {blockCount ? matrix.row_data(row) : nullptr};
            const RowView rowView {rowBlocks, blockCount, matrix.colSize};
            bool parity {false};
            if (blockCount) {
                parity = row_parity(rowBlocks, bitBlocks, blockCount);
            }
            emp::block rowMac {_mm_set_epi64x(0, 0)};
            rowView.for_each_set_bit([&](const size_t colIndex) {
                rowMac = _mm_xor_si128(rowMac, authedBits._macs[colIndex]);
            });
            bits[row] = parity;
            macs[row] = rowMac;
        }
        return {std::move(bits), std::move(macs)};
    }

    ITMacBitKeys operator*(const Matrix<bool>& matrix, const ITMacBitKeys& keys) {
        std::vector<emp::block> localKeys;
        localKeys.reserve(matrix.rowSize);
        const size_t blocksPerRow {matrix.blocks_per_row()};

        for (size_t row {0}; row != matrix.rowSize; ++row) {
            const MatrixBlock* rowBlocks {blocksPerRow ? matrix.row_data(row) : nullptr};
            const RowView rowView {rowBlocks, blocksPerRow, matrix.colSize};
            emp::block rowKey {_mm_set_epi64x(0, 0)};
            rowView.for_each_set_bit([&](const size_t colIndex) {
                rowKey = _mm_xor_si128(rowKey, keys._localKeys[colIndex]);
            });
            localKeys.push_back(rowKey);
        }
        return {std::move(localKeys), keys._globalKeys};
    }
}

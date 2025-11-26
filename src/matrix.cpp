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
}

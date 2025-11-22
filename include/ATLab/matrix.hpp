#ifndef ATLab_MATRIX_HPP
#define ATLab_MATRIX_HPP

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/dynamic_bitset.hpp>

#include "utils.hpp"

namespace ATLab {
    template<class T>
    struct Matrix {
        using Storage = std::vector<T>;

        size_t rowSize {0};
        size_t colSize {0};
        Storage data;

        Matrix() = default;

        Matrix(size_t rows, size_t cols, Storage storage):
            rowSize(rows),
            colSize(cols),
            data(std::move(storage))
        {
#ifdef DEBUG
            if (data.size() != rowSize * colSize) {
                std::ostringstream sout;
                sout << "Matrix storage size mismatch: expected "
                     << rowSize * colSize << ", got " << data.size() << " blocks.\n";
                throw std::invalid_argument{sout.str()};
            }
#endif // DEBUG
        }

        const T& at(const size_t i, const size_t j) const {
#ifdef DEBUG
            if (i >= rowSize || j >= colSize) {
                std::ostringstream sout;
                sout << "Trying to access (" << i << ", " << j << "), "
                    << "but Matrix is of size (" << rowSize << ", " << colSize << ").\n";
                throw std::invalid_argument{sout.str()};
            }
#endif // DEBUG
            return data[i * colSize + j];
        }
    };

    template<>
    struct Matrix<bool> {
        using Block64 = uint64_t;
        static constexpr size_t bits_per_block {sizeof(Block64) * 8};

        size_t rowSize {0};
        size_t colSize {0};
        std::vector<Block64> data;

        Matrix() = default;
        Matrix(Matrix&&) = default;

        Matrix(const size_t rows, const size_t cols, std::vector<Block64> storage):
            rowSize(rows),
            colSize(cols),
            data(std::move(storage))
        {
#ifdef DEBUG
            if (data.size() != Total_block_count(rows, cols)) {
                std::ostringstream sout;
                sout << "Matrix<bool> storage size mismatch: expected "
                    << Total_block_count(rows, cols) << ", got " << data.size() << " blocks.\n";
                throw std::invalid_argument{sout.str()};
            }
#endif // DEBUG
        }

        static constexpr size_t Blocks_per_row(const size_t cols) {
            return cols ? ((cols + bits_per_block - 1) / bits_per_block) : 0;
        }

        size_t blocks_per_row() const {
            return Blocks_per_row(colSize);
        }

        static constexpr size_t Total_block_count(const size_t rows, const size_t cols) {
            return rows * Blocks_per_row(cols);
        }

        const Block64* row_data(const size_t row) const {
#ifdef DEBUG
            if (row >= rowSize) {
                std::ostringstream sout;
                sout << "Trying to access row " << row
                    << " but matrix has only " << rowSize << " rows.\n";
                throw std::invalid_argument{sout.str()};
            }
#endif // DEBUG
            const size_t blockPerRow {blocks_per_row()};
            return blockPerRow ? data.data() + row * blockPerRow : data.data();
        }

        Block64* row_data(const size_t row) {
#ifdef DEBUG
            if (row >= rowSize) {
                std::ostringstream sout;
                sout << "Trying to access row " << row
                    << " but matrix has only " << rowSize << " rows.\n";
                throw std::invalid_argument{sout.str()};
            }
#endif // DEBUG
            const size_t blockPerRow {blocks_per_row()};
            return blockPerRow ? data.data() + row * blockPerRow : data.data();
        }

        bool at(const size_t i, const size_t j) const {
#ifdef DEBUG
            if (i >= rowSize || j >= colSize) {
                std::ostringstream sout;
                sout << "Trying to access (" << i << ", " << j << "), "
                    << "but Matrix<bool> is of size (" << rowSize << ", " << colSize << ").\n";
                throw std::invalid_argument{sout.str()};
            }
#endif // DEBUG
            const size_t blocksPerRow {blocks_per_row()};
            if (!blocksPerRow) {
                return false;
            }
            const size_t blockPos {(i * blocksPerRow) + (j / bits_per_block)};
            const size_t bitOffset {j % bits_per_block};
            return (data[blockPos] >> bitOffset) & 1U;
        }
    };

    using MatrixBlock = Matrix<bool>::Block64;

    inline size_t calc_bitset_blockSize(const size_t bitSize) {
        constexpr size_t bitsPerBlock {Matrix<bool>::bits_per_block};
        return (bitSize + bitsPerBlock - 1) / bitsPerBlock;
    }

    inline size_t calc_matrix_blockSize(const size_t rows, const size_t cols) {
        return Matrix<bool>::Total_block_count(rows, cols);
    }

    inline void zero_matrix_row_padding(std::vector<Matrix<bool>::Block64>& blocks,
                                        const size_t rows,
                                        const size_t cols) {
        const size_t blockPerRow {Matrix<bool>::Blocks_per_row(cols)};
        if (!blockPerRow) {
            return;
        }
        const size_t validBits {cols % Matrix<bool>::bits_per_block};
        if (!validBits) {
            return;
        }
        const Matrix<bool>::Block64 mask {(Matrix<bool>::Block64{1} << validBits) - 1};
        for (size_t row {0}; row < rows; ++row) {
            blocks[row * blockPerRow + (blockPerRow - 1)] &= mask;
        }
    }
}

#endif // ATLab_MATRIX_HPP

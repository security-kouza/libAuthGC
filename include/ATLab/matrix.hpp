#ifndef ATLab_MATRIX_HPP
#define ATLab_MATRIX_HPP

#include <cstddef>
#include <type_traits>

#include <boost/dynamic_bitset.hpp>

#include "utils.hpp"

namespace ATLab {
    template<class T>
    struct Matrix {
        using Storage = std::conditional_t<
            std::is_same_v<T, bool>, Bitset, std::vector<T>
        >;

        const size_t rowSize, colSize;
        Storage data;

        auto at(const size_t i, const size_t j) const {
#ifdef DEBUG
            if (i >= rowSize || j >= colSize) {
                std::ostringstream sout;
                sout << "Trying to access (" << i << ", " << j << "), "
                    << "but Matrix is of size (" << rowSize << ", " << colSize << ".\n";
                throw std::invalid_argument{sout.str()};
            }
#endif // DEBUG
            return data[i * colSize + j];
        }
    };

    using BitsetBlock = Matrix<bool>::Storage::block_type;

    inline size_t calc_bitset_blockSize(const size_t bitSize) {
        constexpr size_t bitsPerBlock {Matrix<bool>::Storage::bits_per_block};
        const size_t blockSize {bitSize / bitsPerBlock + static_cast<size_t>(bitSize % bitsPerBlock != 0)};
        return blockSize;
    }
}

#endif // ATLab_MATRIX_HPP

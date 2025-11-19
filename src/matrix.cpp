#include <ATLab/matrix.hpp>

#include <cassert>
#include <boost/core/span.hpp>

#include <authed_bit.hpp>

namespace ATLab {
    namespace {
        class BitView {
            const Bitset& _bits;
            const size_t _offset;
            const size_t _size;
        public:
            BitView(const Bitset& bits, const size_t offset, const size_t size):
                _bits(bits), _offset(offset), _size(size)
            {
#ifdef DEBUG
                assert(_offset + size <= bits.size());
#endif // DEBUG
            }

            size_t size() const {
                return _size;
            }

            bool operator[](const size_t pos) const {
#ifdef DEBUG
                assert(pos < _size);
#endif // DEBUG
                return _bits[_offset + pos];
            }
        };

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

            for (size_t rowIter {0}; rowIter != matrix.rowSize; ++rowIter) {
                for (size_t colIter {0}; colIter != matrix.colSize; ++colIter) {
                    res[rowIter] ^= (matrix.at(rowIter, colIter) && bits[colIter]);
                }
            }
            return res;
        }

        // inner product
        emp::block operator*(const BitView& bits, const std::vector<emp::block>& blocks) {
#ifdef DEBUG
            if (blocks.size() != bits.size()) {
                throw std::invalid_argument{"Sizes of `blocks` and `bits` differ."};
            }
#endif // DEBUG
            const size_t size {bits.size()};

            emp::block sum {_mm_set_epi64x(0, 0)};
            for (size_t i {0}; i < size; ++i) {
                // 0 => 0x0000...; 1 => 0xFFFF....
                const emp::block mask = _mm_set1_epi64x(-static_cast<uint64_t>(bits[i]));

                sum = _mm_xor_si128(sum, _mm_and_si128(blocks[i], mask));
            }
            return sum;
        }
    }

    ITMacBits operator*(const Matrix<bool>& matrix, const ITMacBits& authedBits) {
#ifdef DEBUG
        if (authedBits.global_key_size() != 1) {
            std::ostringstream sout;
            sout << "authedBits has " << authedBits.global_key_size() << " global keys. Only accepting 1.\n";
            throw std::invalid_argument{sout.str()};
        }
#endif // DEBUG
        auto bits {matrix * authedBits._bits};

        std::vector<emp::block> macs;
        macs.reserve(matrix.rowSize);
        for (size_t row {0}; row != matrix.rowSize; ++row) {
            const BitView bitRow {matrix.data, row * matrix.colSize, matrix.colSize};
            macs.push_back(bitRow * authedBits._macs);
        }
        return {std::move(bits), std::move(macs)};
    }

    ITMacBitKeys operator*(const Matrix<bool>& matrix, const ITMacBitKeys& keys) {
        std::vector<emp::block> localKeys;
        localKeys.reserve(matrix.rowSize);
        for (size_t row {0}; row != matrix.rowSize; ++row) {
            const BitView bitRow {matrix.data, row * matrix.colSize, matrix.colSize};
            localKeys.push_back(bitRow * keys._localKeys);
        }
        return {std::move(localKeys), keys._globalKeys};
    }
}

#include "../include/ATLab/utils.hpp"

#include <array>
#include <iostream>
#include <iomanip>

#include <emp-tool/utils/f2k.h>

namespace {
    template <class BitArray>
    emp::block make_block_from_bits_128(BitArray bit) {
        uint64_t high {0}, low {0};
        for (int i = 0; i != 64; ++i) {
            low  |= (static_cast<uint64_t>(bit[i]) << i);
            high |= (static_cast<uint64_t>(bit[i + 64]) << i);
        }
        return emp::makeBlock(high, low);
    }
}

namespace ATLab {
    std::mutex PrintMutex;

    void print_log (const std::string& msg) {
#ifdef DEBUG
        PrintMutex.lock();
        std::clog << "[DEBUG] " << msg << '\n';
        PrintMutex.unlock();
#endif // DEBUG
    }

    /**
     * Not thread safe
     * Please lock `PrintMutex` before calling if needed.
     */
    void print_bytes (const void* const buf, const size_t N, std::ostream& print) {
        // 00: xx xx xx xx xx xx xx xx | xx xx xx xx xx xx xx xx
        // 01: xx xx xx xx xx xx xx xx | xx xx xx xx xx xx xx xx
        int hexLen {0};
        for (auto tmp {N}; tmp > 0; tmp /= 16) {
            ++hexLen;
        }

        const std::ios originalFormat {print.rdbuf()};

        // originalFormat.copyfmt(print);
        print << std::setfill('0') << std::hex;
        for (size_t i {0}; i != N; ++i) {
            if (i % 16 == 0) {
                if (i > 0) {
                    print << '\n';
                }
                print << std::setw(hexLen) << i << ':';
            } else if (i % 8 == 0) {
                print << " |";
            }

            print << ' ' << std::setw(2) << +static_cast<const unsigned char*>(buf)[i];
        }
        print << '\n';
        print.copyfmt(originalFormat);

    }

    emp::block Block(const std::array<bool, 128>& bits) {
        return make_block_from_bits_128(bits);
    }

    emp::block Block(const Bitset& bits) {
        if (bits.size() != 128) {
            throw std::invalid_argument("vector must contain exactly 128 bits");
        }
        return make_block_from_bits_128(bits);
    }

    emp::block Block(const bool* bits) {
        return make_block_from_bits_128(bits);
    }

    emp::block Block(const std::bitset<128>&bits) {
        return make_block_from_bits_128(bits);
    }

    Bitset to_bool_vector(const emp::block& block) {
        const uint64_t low = _mm_extract_epi64(block, 0);
        const uint64_t high = _mm_extract_epi64(block, 1);

        Bitset bits(128);
        for (int i = 0; i != 64; ++i) {
            bits[i] = (low >> i) & 1;
            bits[i + 64] = (high >> i) & 1;
        }
        return bits;
    }

    emp::block polyval(const emp::block* coeff) {
        emp::block res;
        const static emp::GaloisFieldPacking encoder;
        encoder.packing(&res, coeff);
        return res;
    }

    emp::block gf_inverse(const emp::block& x) {
        const emp::block kOne {_mm_set_epi64x(0, 1)};
        auto square {[](const emp::block& value) {
            return gf_mul_block(value, value);
        }};
        auto square_times {[&square](emp::block value, const size_t times) {
            for (size_t i {0}; i < times; ++i) {
                value = square(value);
            }
            return value;
        }};

        emp::block minusOne {x}; // x^{2^1 - 1}
        emp::block minusTwo {kOne}; // x^{2^1 - 2} = 1

        for (size_t k {1}; k < 128; k <<= 1) {
            const emp::block powered {square_times(minusOne, k)};
            minusOne = gf_mul_block(powered, minusOne);
            minusTwo = gf_mul_block(powered, minusTwo);
        }

        return minusTwo;
    }
}


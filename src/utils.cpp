#include "utils.hpp"
#include "PRNG.hpp"

#include <array>
#include <iostream>
#include <iomanip>

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
        auto res {static_cast<__uint128_t>(0)};
        for (auto rit {bits.crbegin()}; rit != bits.crend(); ++rit) {
            res <<= 1;
            res |= *rit;
        }
        return as_block(res);
    }

    const emp::block& as_block(const __uint128_t& i128) {
        return *reinterpret_cast<const emp::block*>(&i128);
    }

    const __uint128_t& as_uint128(const emp::block& block) {
        return *reinterpret_cast<const __uint128_t*>(&block);
    }
}

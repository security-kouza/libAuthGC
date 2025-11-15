/*
This file is part of EOTKyber of Abe-Tibouchi Laboratory, Kyoto University
Copyright (C) 2023-2024  Kyoto University
Copyright (C) 2023-2024  Peihao Li <li.peihao.62s@st.kyoto-u.ac.jp>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef REVELIO_UTILS
#define REVELIO_UTILS

#include <bitset>
#include <mutex>
#include <iostream>
#include <vector>
#include <utility>
#include <emp-tool/utils/block.h>
#include <emp-tool/utils/f2k.h>

#include "PRNG.hpp"

namespace ATLab {
    static_assert(sizeof(emp::block) == sizeof(__uint128_t));

    // Must be locked before printing anything
    // (No big deal. Just for debug message format)
    extern std::mutex PrintMutex;

    // print log if DEBUG, otherwise do nothing
    void print_log (const std::string& msg);

    /**
     * Output example:
     * 00: xx xx xx xx xx xx xx xx | xx xx xx xx xx xx xx xx
     * 01: xx xx xx xx xx xx xx xx | xx xx xx xx xx xx xx xx
     */
    void print_bytes (const void* buf, size_t N, std::ostream& = std::clog);

    template <size_t N>
    void print_bytes(const std::array<std::byte, N>& buf) {
        print_bytes(buf.data(), N);
    }

    // Little Endian
    emp::block Block(const std::array<bool, 128>&);

    // Little Endian
    emp::block Block(const std::vector<bool>&);

    // Little Endian
    emp::block Block(const bool*);

    // Little Endian
    emp::block Block(const std::bitset<128>&);

    // Little Endian
    std::vector<bool> to_bool_vector(const emp::block&);

    inline emp::block as_block(const __uint128_t& i128) {
        const auto lo {static_cast<uint64_t>(i128)};
        const auto hi {static_cast<uint64_t>(i128 >> 64)};
        return _mm_set_epi64x(static_cast<long long>(hi), static_cast<long long>(lo));
    }

    inline __uint128_t as_uint128(emp::block b) {
        const auto lo {static_cast<uint64_t>(_mm_cvtsi128_si64(b))};
        const auto hi {static_cast<uint64_t>(_mm_extract_epi64(b, 1))};
        return (static_cast<__uint128_t>(hi) << 64) | lo;
    }

    // const emp::block& as_block(const __uint128_t&);
    // const __uint128_t& as_uint128(const emp::block&);

    namespace detail {
        inline emp::block gf_mul_block(const emp::block& lhs, const emp::block& rhs) {
            emp::block out;
            emp::gfmul(lhs, rhs, &out);
            return out;
        }

        template <size_t... Indices>
        inline emp::block vector_inner_product_impl(const emp::block* a, const emp::block* b, std::index_sequence<Indices...>) {
            emp::block accumulator = emp::zero_block;
            ((accumulator = accumulator ^ gf_mul_block(a[Indices], b[Indices])), ...);
            return accumulator;
        }
    }

    template<size_t N>
    emp::block vector_inner_product(const emp::block* a, const emp::block* b) {
        if constexpr (N == 0) {
            return emp::zero_block;
        } else {
            return detail::vector_inner_product_impl(a, b, std::make_index_sequence<N>{});
        }
    }

    /**
     * Computes the polynomial evaluation using the provided coefficients in a Galois field.
     * @param coeff Pointer to an array of 128 coefficients. Little-endian required (coeff[i] * X^i).
     */
    emp::block polyval(const emp::block* coeff);

    inline bool get_LSB(const __m128i& x) {
        // Fast when the __V is already in a XMM register.
        return _mm_testz_si128(x, _mm_cvtsi32_si128(1)) == 0;
    }
}

#endif // REVELIO_UTILS

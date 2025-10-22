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

#include <mutex>
#include <iostream>
#include <vector>
#include <emp-tool/utils/block.h>

#include "PRNG.hpp"

namespace ATLab {
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

    // conversion to emp::block (__mm128i)
    emp::block Block(const std::array<bool, 128>&);

    static_assert(sizeof(emp::block) == sizeof(__uint128_t));
    const emp::block& as_block(const __uint128_t&);
    const __uint128_t& as_uint128(const emp::block&);
}

#endif // REVELIO_UTILS

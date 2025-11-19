/*
This file is part of EOTKyber of Abe-Tibouchi Laboratory, Kyoto University
Copyright © 2023-2024  Kyoto University
Copyright © 2023-2024  Peihao Li <li.peihao.62s@st.kyoto-u.ac.jp>

EOTKyber is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

EOTKyber is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "PRNG.hpp"

#include <cassert>
#include <fstream>
#include <random>

#include "ATLab/matrix.hpp"

extern "C" {
#include <rng.h>
}

namespace ATLab {
    emp::PRG THE_GLOBAL_PRNG;

    PRNG_Kyber& PRNG_Kyber::get_PRNG_Kyber() {
        static PRNG_Kyber KyberInstance; // state stored in rng.c
        static bool firstInvocation {true};
        if (!firstInvocation) {
            return KyberInstance;
        }

        constexpr size_t SEED_BYTES {48};
        constexpr size_t INT_ARR_SIZE {SEED_BYTES / sizeof(unsigned int)};
        std::array<unsigned int, INT_ARR_SIZE> seed {};
        std::random_device rd{"/dev/urandom"};

        for (auto& r : seed) {
            r = rd();
        }

#ifdef DEBUG_FIXED_SEED
        seed = {0};
#endif // DEBUG_FIXED_SEED

        // needs a 48-byte-long seed
        randombytes_init(
            reinterpret_cast<unsigned char*>(seed.data()),
            nullptr,
            0 // since `security_strength` is ignored. Bad design :(
        );
        firstInvocation = false;
        return KyberInstance;
    }

    // ReSharper disable once CppMemberFunctionMayBeConst
    PRNG_Kyber::result_type PRNG_Kyber::operator()() {
        std::array<uint8_t, 16> buf {};
        randombytes(buf.data(), buf.size());
        return *reinterpret_cast<result_type*>(buf.data());
    }

    Bitset random_dynamic_bitset(const size_t bitSize) {
        const size_t blockSize {calc_bitset_blockSize(bitSize)};
        std::vector<BitsetBlock> rawData(blockSize);
        THE_GLOBAL_PRNG.random_data(rawData.data(), rawData.size() * sizeof(BitsetBlock));
        Bitset res {rawData.cbegin(), rawData.cend()};
        res.resize(bitSize);
        return res;
    }
}

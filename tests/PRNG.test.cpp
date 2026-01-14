#include <array>
#include <iostream>

#include <gtest/gtest.h>

#include "../include/ATLab/PRNG.hpp"

TEST(PRNG, Default_Seed) {
#ifdef DEBUG_FIXED_SEED
    using namespace ATLab;
    auto prng {PRNG_Kyber::get_PRNG_Kyber()};
    const auto randInt {prng()};

    constexpr std::array<uint8_t, 16> expectedRandIntBytes {
        0x91, 0x61, 0x8f, 0xe9, 0x9a, 0x8f, 0x94, 0x20, 0x49, 0x7b, 0x24, 0x6f, 0x73, 0x5b, 0x27, 0xa0
    };
    EXPECT_EQ(*reinterpret_cast<const PRNG_Kyber::result_type*>(expectedRandIntBytes.data()), randInt);

    const auto secondRandInt {PRNG_Kyber::get_PRNG_Kyber()()};
    EXPECT_NE(randInt, secondRandInt);
#else
    std::clog << "Seed is randomized. Skipping...\n";
#endif // DEBUG_FIXED_SEED
}

#include <array>
#include <cstddef>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

#include "../include/ATLab/utils.hpp"

TEST(Utils, PrintByte) {
    // Static Test
    constexpr std::array<uint8_t, 17> testBytes {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
    };
    const std::string expectedOutput {"00: 00 01 02 03 04 05 06 07 | 08 09 0a 0b 0c 0d 0e 0f\n10: 10\n"};
    std::ostringstream sout;
    ATLab::print_bytes(testBytes.data(), 17, sout);
    EXPECT_EQ(expectedOutput, sout.str());

    // Dynamic Test by showing
    constexpr size_t len {300};
    std::array<std::byte, len> buf {};
    std::fstream fout {"/dev/urandom"};
    fout.read(reinterpret_cast<char*>(buf.data()), len);

    ATLab::print_bytes(buf);
}

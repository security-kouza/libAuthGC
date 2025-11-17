#include <gtest/gtest.h>

#include <circuit_parser.hpp>

TEST(Circuit_Parser, default) {
    EXPECT_NO_THROW(
        const auto circuit {ATLab::Circuit("circuits/bristol_format/sha-1.txt")};
    );
}

#ifndef ATLab_IT_MAC_CHECK_HPP
#define ATLab_IT_MAC_CHECK_HPP

#include "../include/authed_bit.hpp"
#include <gtest/gtest.h>
#include <vector>
#include <utility>

const std::string ADDRESS {"127.0.0.1"};
constexpr unsigned short PORT {12341};

using ATLab::ITMacBitKeys;
using ATLab::ITMacBits;

static ATLab::NetIO& server_io() {
    static ATLab::NetIO io(ATLab::NetIO::SERVER, ADDRESS, PORT, true);
    return io;
}

static ATLab::NetIO& client_io() {
    static ATLab::NetIO io(ATLab::NetIO::CLIENT, ADDRESS, PORT, true);
    return io;
}


inline void test_ITMacBits(
    const std::vector<std::pair<const ITMacBits*, const ITMacBitKeys*>>& testPairs
) {
    for (const auto& [bitsPtr, keysPtr] : testPairs) {
        ASSERT_NE(bitsPtr, nullptr);
        ASSERT_NE(keysPtr, nullptr);

        const auto& bits {*bitsPtr};
        const auto& keys {*keysPtr};

        EXPECT_EQ(bits.size(), keys.size());
        if (bits.size()) {
            EXPECT_EQ(bits.global_key_size(), keys.global_key_size());
            const size_t globalKeySize {bits.global_key_size()};

            for (size_t i = 0; i < globalKeySize; ++i) {
                for (size_t j = 0; j < bits.size(); ++j) {
                    emp::block expected = keys.get_local_key(i, j);
                    if (bits.at(j)) {
                        expected = expected ^ keys.get_global_key(i);
                    }
                    EXPECT_EQ(ATLab::as_uint128(expected), ATLab::as_uint128(bits.get_mac(i, j)));
                }
            }
        }
    }
}

#endif // ATLab_IT_MAC_CHECK_HPP

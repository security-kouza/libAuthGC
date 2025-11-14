#include <gtest/gtest.h>

#include <iostream>
#include <thread>
#include <mutex>

#include <emp-tool/utils/block.h>
#include <emp-tool/utils/f2k.h>
#include <emp-tool/utils/utils.h>

#include <authed_bit.hpp>


const std::string ADDRESS {"127.0.0.1"};
constexpr unsigned short PORT {12345};

TEST(Authed_Bit, random_bits) {
    constexpr size_t deltaSize {3};
    constexpr size_t BitSize {128};

    std::vector<emp::block> deltaArr(deltaSize);
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& delta : deltaArr) {
        delta = ATLab::as_block(prng());
    }

    std::unique_ptr<ATLab::ITMacKeys> keys;
    std::unique_ptr<ATLab::ITMacBits> bits;

    std::thread bCOTSenderThread{
                    [&]() {
                        emp::NetIO io(emp::NetIO::SERVER, ADDRESS, PORT, true);
                        ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
                        keys = std::make_unique<ATLab::ITMacKeys>(sender, BitSize);
                    }
                }, bCOTReceiverThread{
                    [&]() {
                        emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, PORT, true);
                        ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
                        bits = std::make_unique<ATLab::ITMacBits>(receiver, BitSize);
                    }
                };

    bCOTSenderThread.join();
    bCOTReceiverThread.join();

    ASSERT_EQ(keys->size(), BitSize);
    ASSERT_EQ(bits->size(), BitSize);
    ASSERT_EQ(keys->global_key_size(), deltaSize);
    ASSERT_EQ(bits->global_key_size(), deltaSize);

    for (size_t i {0}; i != deltaSize; ++i) {
        ASSERT_EQ(ATLab::as_uint128(keys->get_global_key(i)), ATLab::as_uint128(deltaArr.at(i)));
    }

    for (size_t i = 0; i < deltaSize; ++i) {
        for (size_t j = 0; j < BitSize; ++j) {
            emp::block expected = keys->get_local_key(j, i);
            if (bits->at(j)) {
                expected = expected ^ deltaArr.at(i);
            }
            EXPECT_EQ(ATLab::as_uint128(expected), ATLab::as_uint128(bits->get_mac(j, i)));
        }
    }
}

TEST(Authed_Bit, fixed_bits) {
    constexpr size_t deltaSize {3};
    constexpr size_t BitSize {128};

    std::vector<emp::block> deltaArr(deltaSize);
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& delta : deltaArr) {
        delta = ATLab::as_block(prng());
    }

    std::vector<bool> bitsToFix {ATLab::random_bool_vector(BitSize)};


    std::unique_ptr<ATLab::ITMacKeys> keys;
    std::unique_ptr<ATLab::ITMacBits> bits;

    std::thread bCOTSenderThread{
        [&]() {
            emp::NetIO io(emp::NetIO::SERVER, ADDRESS, PORT, true);
            ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
            keys = std::make_unique<ATLab::ITMacKeys>(io, sender, BitSize);
        }
    }, bCOTReceiverThread{
        [&]() {
            emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, PORT, true);
            ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
            bits = std::make_unique<ATLab::ITMacBits>(io, receiver, bitsToFix);
        }
    };

    bCOTSenderThread.join();
    bCOTReceiverThread.join();

    ASSERT_EQ(keys->size(), BitSize);
    ASSERT_EQ(bits->size(), BitSize);
    ASSERT_EQ(keys->global_key_size(), deltaSize);
    ASSERT_EQ(bits->global_key_size(), deltaSize);

    for (size_t i {0}; i != BitSize; ++i) {
        ASSERT_EQ(bits->at(i), bitsToFix.at(i));
    }

    for (size_t i {0}; i != deltaSize; ++i) {
        ASSERT_EQ(ATLab::as_uint128(keys->get_global_key(i)), ATLab::as_uint128(deltaArr.at(i)));
    }

    for (size_t i = 0; i < deltaSize; ++i) {
        for (size_t j = 0; j < BitSize; ++j) {
            emp::block expected = keys->get_local_key(j, i);
            if (bits->at(j)) {
                expected = expected ^ deltaArr.at(i);
            }
            EXPECT_EQ(ATLab::as_uint128(expected), ATLab::as_uint128(bits->get_mac(j, i)));
        }
    }
}

TEST(Authed_Bit, authed_blocks) {
    constexpr size_t deltaSize {3};
    constexpr size_t BitSize {128};

    std::vector<emp::block> deltaArr(deltaSize);
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& delta : deltaArr) {
        delta = ATLab::as_block(prng());
    }

    std::unique_ptr<ATLab::ITMacKeys> keys;
    std::unique_ptr<ATLab::ITMacBits> bits;

    std::thread bCOTSenderThread{
        [&]() {
            emp::NetIO io(emp::NetIO::SERVER, ADDRESS, PORT, true);
            ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
            keys = std::make_unique<ATLab::ITMacKeys>(sender, BitSize);
        }
    }, bCOTReceiverThread{
        [&]() {
            emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, PORT, true);
            ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
            bits = std::make_unique<ATLab::ITMacBits>(receiver, BitSize);
        }
    };

    bCOTSenderThread.join();
    bCOTReceiverThread.join();

    const auto BlockKeys {std::move(*keys).polyval_to_Blocks()};
    const auto AuthedBlocks {std::move(*bits).polyval_to_Blocks()};

    for (size_t i {0}; i != deltaSize; ++i) {
        ASSERT_EQ(ATLab::as_uint128(deltaArr.at(i)), ATLab::as_uint128(BlockKeys.get_global_key(i)));
    }

    emp::block tmpProd;
    for (size_t i {0}; i != deltaSize; ++i) {
        emp::gfmul(deltaArr.at(i), AuthedBlocks.block, &tmpProd);
        const auto expected {ATLab::as_uint128(BlockKeys.get_local_key(i) ^ tmpProd)};
        EXPECT_EQ(expected, ATLab::as_uint128(AuthedBlocks.get_mac(i)));
    }
}

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

    std::unique_ptr<ATLab::ITMacBitKeys> keys;
    std::unique_ptr<ATLab::ITMacBits> bits;

    std::thread bCOTSenderThread{
                    [&]() {
                        emp::NetIO io(emp::NetIO::SERVER, ADDRESS, PORT, true);
                        ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
                        keys = std::make_unique<ATLab::ITMacBitKeys>(sender, BitSize);
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
            emp::block expected = keys->get_local_key(i, j);
            if (bits->at(j)) {
                expected = expected ^ deltaArr.at(i);
            }
            EXPECT_EQ(ATLab::as_uint128(expected), ATLab::as_uint128(bits->get_mac(i, j)));
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


    std::unique_ptr<ATLab::ITMacBitKeys> keys;
    std::unique_ptr<ATLab::ITMacBits> bits;

    std::thread bCOTSenderThread{
        [&]() {
            emp::NetIO io(emp::NetIO::SERVER, ADDRESS, PORT, true);
            ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
            keys = std::make_unique<ATLab::ITMacBitKeys>(io, sender, BitSize);
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
            emp::block expected = keys->get_local_key(i, j);
            if (bits->at(j)) {
                expected = expected ^ deltaArr.at(i);
            }
            EXPECT_EQ(ATLab::as_uint128(expected), ATLab::as_uint128(bits->get_mac(i, j)));
        }
    }
}

TEST(Authed_Bit, random_blocks) {
    constexpr size_t deltaSize {3};
    constexpr size_t blockSize {40};
    constexpr unsigned short BLOCK_PORT {static_cast<unsigned short>(PORT + 2)};

    std::vector<emp::block> deltaArr(deltaSize);
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& delta : deltaArr) {
        delta = ATLab::as_block(prng());
    }

    std::unique_ptr<ATLab::ITMacBlockKeys> keys;
    std::unique_ptr<ATLab::ITMacBlocks> authedBlocks;

    std::thread bCOTSenderThread{
        [&]() {
            emp::NetIO io(emp::NetIO::SERVER, ADDRESS, BLOCK_PORT, true);
            ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
            keys = std::make_unique<ATLab::ITMacBlockKeys>(sender, blockSize);
        }
    }, bCOTReceiverThread{
        [&]() {
            emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, BLOCK_PORT, true);
            ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
            authedBlocks = std::make_unique<ATLab::ITMacBlocks>(receiver, blockSize);
        }
    };

    bCOTSenderThread.join();
    bCOTReceiverThread.join();

    ASSERT_EQ(keys->size(), blockSize);
    ASSERT_EQ(authedBlocks->size(), blockSize);
    ASSERT_EQ(keys->global_key_size(), deltaSize);
    ASSERT_EQ(authedBlocks->global_key_size(), deltaSize);

    for (size_t i {0}; i != deltaSize; ++i) {
        ASSERT_EQ(ATLab::as_uint128(deltaArr.at(i)), ATLab::as_uint128(keys->get_global_key(i)));
    }

    emp::block tmpProd;
    for (size_t deltaIter {0}; deltaIter != deltaSize; ++deltaIter) {
        for (size_t blockIter {0}; blockIter != blockSize; ++blockIter) {
            emp::gfmul(deltaArr.at(deltaIter), authedBlocks->get_block(blockIter), &tmpProd);
            const auto expected {
                ATLab::as_uint128(keys->get_local_key(deltaIter, blockIter) ^ tmpProd)
            };
            EXPECT_EQ(expected, ATLab::as_uint128(authedBlocks->get_mac(deltaIter, blockIter)));
        }
    }
}

TEST(Authed_Bit, fixed_blocks) {
    constexpr size_t deltaSize {3};
    constexpr size_t blockSize {40};
    constexpr unsigned short LOCAL_PORT {static_cast<unsigned short>(PORT + 1)};

    std::vector<emp::block> deltaArr(deltaSize);
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& delta : deltaArr) {
        delta = ATLab::as_block(prng());
    }

    std::vector<emp::block> blocksToFix(blockSize);
    for (auto& block : blocksToFix) {
        block = ATLab::as_block(prng());
    }

    std::unique_ptr<ATLab::ITMacBlockKeys> keys;
    std::unique_ptr<ATLab::ITMacBlocks> authedBlocks;

    std::thread bCOTSenderThread{
        [&]() {
            emp::NetIO io(emp::NetIO::SERVER, ADDRESS, LOCAL_PORT, true);
            ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
            keys = std::make_unique<ATLab::ITMacBlockKeys>(io, sender, blockSize);
        }
    }, bCOTReceiverThread{
        [&, blocksToFix]() {
            emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, LOCAL_PORT, true);
            ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
            authedBlocks = std::make_unique<ATLab::ITMacBlocks>(io, receiver, blocksToFix);
        }
    };

    bCOTSenderThread.join();
    bCOTReceiverThread.join();

    ASSERT_EQ(keys->size(), blockSize);
    ASSERT_EQ(authedBlocks->size(), blockSize);
    ASSERT_EQ(keys->global_key_size(), deltaSize);
    ASSERT_EQ(authedBlocks->global_key_size(), deltaSize);

    for (size_t blockIter {0}; blockIter != blockSize; ++blockIter) {
        EXPECT_EQ(
            ATLab::as_uint128(blocksToFix.at(blockIter)),
            ATLab::as_uint128(authedBlocks->get_block(blockIter))
        );
    }

    for (size_t i {0}; i != deltaSize; ++i) {
        EXPECT_EQ(ATLab::as_uint128(deltaArr.at(i)), ATLab::as_uint128(keys->get_global_key(i)));
    }

    emp::block tmpProd;
    for (size_t deltaIter {0}; deltaIter != deltaSize; ++deltaIter) {
        for (size_t blockIter {0}; blockIter != blockSize; ++blockIter) {
            emp::gfmul(deltaArr.at(deltaIter), authedBlocks->get_block(blockIter), &tmpProd);
            const auto expected {
                ATLab::as_uint128(keys->get_local_key(deltaIter, blockIter) ^ tmpProd)
            };
            EXPECT_EQ(expected, ATLab::as_uint128(authedBlocks->get_mac(deltaIter, blockIter)));
        }
    }
}

#include <gtest/gtest.h>

#include <iostream>
#include <thread>
#include <mutex>
#include <type_traits>
#include <map>

#include <emp-tool/utils/block.h>
#include <emp-tool/utils/f2k.h>
#include <emp-tool/utils/utils.h>

#include <ATLab/authed_bit.hpp>
#include "test-helper.hpp"
#include "ATLab/hash_wrapper.h"

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

    std::thread bCOTSenderThread{[&]() {
        auto& io {server_io()};
        ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
        keys = std::make_unique<ATLab::ITMacBitKeys>(sender, BitSize);
        io.flush();
    }}, bCOTReceiverThread{[&]() {
        auto& io {client_io()};
        ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
        bits = std::make_unique<ATLab::ITMacBits>(receiver, BitSize);
        io.flush();
    }};

    bCOTSenderThread.join();
    bCOTReceiverThread.join();

    ASSERT_EQ(keys->size(), BitSize);
    ASSERT_EQ(bits->size(), BitSize);
    ASSERT_EQ(keys->global_key_size(), deltaSize);
    ASSERT_EQ(bits->global_key_size(), deltaSize);

    for (size_t i {0}; i != deltaSize; ++i) {
        ASSERT_EQ(ATLab::as_uint128(keys->get_global_key(i)), ATLab::as_uint128(deltaArr.at(i)));
    }

    test_ITMacBits({{bits.get(), keys.get()}});
}

TEST(Authed_Bit, fixed_bits) {
    constexpr size_t deltaSize {3};
    constexpr size_t BitSize {128};

    std::vector<emp::block> deltaArr(deltaSize);
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& delta : deltaArr) {
        delta = ATLab::as_block(prng());
    }

    auto bitsToFix {ATLab::random_dynamic_bitset(BitSize)};


    std::unique_ptr<ATLab::ITMacBitKeys> keys;
    std::unique_ptr<ATLab::ITMacBits> bits;

    std::thread bCOTSenderThread{[&]() {
        auto& io {server_io()};
        ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
        keys = std::make_unique<ATLab::ITMacBitKeys>(io, sender, BitSize);
        io.flush();
    }}, bCOTReceiverThread{[&]() {
        auto& io {client_io()};
        ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
        bits = std::make_unique<ATLab::ITMacBits>(io, receiver, bitsToFix);
        io.flush();
    }};

    bCOTSenderThread.join();
    bCOTReceiverThread.join();

    ASSERT_EQ(keys->size(), BitSize);
    ASSERT_EQ(bits->size(), BitSize);
    ASSERT_EQ(keys->global_key_size(), deltaSize);
    ASSERT_EQ(bits->global_key_size(), deltaSize);

    for (size_t i {0}; i != BitSize; ++i) {
        ASSERT_EQ(bits->at(i), bitsToFix.test(i));
    }

    for (size_t i {0}; i != deltaSize; ++i) {
        ASSERT_EQ(ATLab::as_uint128(keys->get_global_key(i)), ATLab::as_uint128(deltaArr.at(i)));
    }

    test_ITMacBits({{bits.get(), keys.get()}});
}

TEST(Authed_Bit, random_blocks) {
    constexpr size_t deltaSize {3};
    constexpr size_t blockSize {40};

    std::vector<emp::block> deltaArr(deltaSize);
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& delta : deltaArr) {
        delta = ATLab::as_block(prng());
    }

    std::unique_ptr<ATLab::ITMacBlockKeys> keys;
    std::unique_ptr<ATLab::ITMacBlocks> authedBlocks;

    std::thread bCOTSenderThread{[&]() {
        auto& io {server_io()};
        ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
        keys = std::make_unique<ATLab::ITMacBlockKeys>(sender, blockSize);
        io.flush();
    }}, bCOTReceiverThread{[&]() {
        auto& io {client_io()};
        ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
        authedBlocks = std::make_unique<ATLab::ITMacBlocks>(receiver, blockSize);
        io.flush();
    }};

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

    std::thread bCOTSenderThread{[&]() {
        auto& io {server_io()};
        ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
        keys = std::make_unique<ATLab::ITMacBlockKeys>(io, sender, blockSize);
        io.flush();
    }}, bCOTReceiverThread{[&, blocksToFix]() {
        auto& io {client_io()};
        ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
        authedBlocks = std::make_unique<ATLab::ITMacBlocks>(io, receiver, blocksToFix);
        io.flush();
    }};

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

TEST(Authed_Bit, fixed_blocks_scalar_bitset) {
    constexpr size_t deltaSize {1};
    constexpr size_t blockSize {40};
    constexpr unsigned short LOCAL_PORT {static_cast<unsigned short>(PORT + 3)};

    std::vector<emp::block> deltaArr(deltaSize);
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& delta : deltaArr) {
        delta = ATLab::as_block(prng());
    }

    const emp::block scalarBlock {ATLab::as_block(prng())};
    ATLab::Bitset blockSelectors {ATLab::random_dynamic_bitset(blockSize)};
    if (!blockSelectors.any()) {
        blockSelectors.set(0);
    }

    std::unique_ptr<ATLab::ITMacBlockKeys> keys;
    std::unique_ptr<ATLab::ITMacScaledBits> authedBlocks;

    std::thread bCOTSenderThread{[&]() {
        auto& io {server_io()};
        ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
        keys = std::make_unique<ATLab::ITMacBlockKeys>(io, sender, blockSize);
        io.flush();
    }}, bCOTReceiverThread{[&, blockSelectors]() {
        auto& io {client_io()};
        ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
        authedBlocks = std::make_unique<ATLab::ITMacScaledBits>(io, receiver, scalarBlock, blockSelectors);
        io.flush();
    }};

    bCOTSenderThread.join();
    bCOTReceiverThread.join();

    ASSERT_EQ(keys->size(), blockSize);
    ASSERT_EQ(keys->global_key_size(), deltaSize);

    ASSERT_NE(authedBlocks, nullptr);
    EXPECT_EQ(authedBlocks->size(), blockSize);
    EXPECT_EQ(authedBlocks->global_key_size(), 1);
    EXPECT_EQ(blockSelectors, authedBlocks->selectors());
    EXPECT_EQ(ATLab::as_uint128(scalarBlock), ATLab::as_uint128(authedBlocks->scalar()));

    for (size_t blockIter {0}; blockIter != blockSize; ++blockIter) {
        const auto expectedBlock {
            blockSelectors.test(blockIter) ? ATLab::as_uint128(scalarBlock) : ATLab::as_uint128(emp::zero_block)
        };
        EXPECT_EQ(expectedBlock, ATLab::as_uint128(authedBlocks->get_block(blockIter)));
    }

    for (size_t i {0}; i != deltaSize; ++i) {
        EXPECT_EQ(ATLab::as_uint128(deltaArr.at(i)), ATLab::as_uint128(keys->get_global_key(i)));
    }

    emp::block tmpProd;
    for (size_t blockIter {0}; blockIter != blockSize; ++blockIter) {
        emp::gfmul(deltaArr.front(), authedBlocks->get_block(blockIter), &tmpProd);
        const auto expected {
            ATLab::as_uint128(keys->get_local_key(0, blockIter) ^ tmpProd)
        };
        EXPECT_EQ(expected, ATLab::as_uint128(authedBlocks->get_mac(blockIter)));
    }
}

TEST(Authed_Bit, open) {
    using namespace ATLab;

    constexpr size_t bitSize {128};
    emp::block globalKey;
    do {
        THE_GLOBAL_PRNG.random_block(&globalKey, 1);
    } while (as_uint128(globalKey) == 0);

    std::array<bool, bitSize> bitArr {};
    Bitset bits(bitSize);
    THE_GLOBAL_PRNG.random_bool(bitArr.data(), bitSize);
    for (size_t i {0}; i != bitSize; ++i) {
        bits.set(i, bitArr.at(i));
    }

    std::vector<emp::block> macs(bitSize), localKeys(bitSize);
    THE_GLOBAL_PRNG.random_block(macs.data(), macs.size());
    for (size_t i {0}; i != macs.size(); ++i) {
        localKeys[i] = _mm_xor_si128(macs[i], and_all_bits(bitArr[i], globalKey));
    }

    const ITMacBits authedBits {bits, std::move(macs)};
    const ITMacBitKeys keys {std::move(localKeys), {globalKey}};

    constexpr size_t SLICE_BEGIN {1}, SLICE_END {bitSize};

    /**
     * Test three times
     * - normal
     * - sliced version
     * - malicious prover with faked macs
     */
    std::thread senderThread {[&]() {
        auto& io {server_io()};
        authedBits.open(io, SHA256::hash_to_128);
        authedBits.open(io, SHA256::hash_to_128, SLICE_BEGIN, SLICE_END);

        std::vector<emp::block> randomMacs(bitSize);
        THE_GLOBAL_PRNG.random_block(randomMacs.data(), randomMacs.size());
        const ITMacBits fakeMacs {bits, std::move(randomMacs)};
        fakeMacs.open(io, SHA256::hash_to_128);

        io.flush();
    }}, receiverThread {[&]() {
        auto& io {client_io()};
        EXPECT_NO_THROW(
            const ITMacOpenedBits opened {keys.open(io, SHA256::hash_to_128)};
            for (size_t i {0}; i != bitSize; ++i) {
                EXPECT_EQ(bitArr[i], opened.test(i));
            }
        );

        EXPECT_NO_THROW(
            const ITMacOpenedBits opened {keys.open(io, SHA256::hash_to_128, SLICE_BEGIN, SLICE_END)};
            for (size_t i {0}; i != SLICE_END - SLICE_BEGIN; ++i) {
                EXPECT_EQ(bitArr[SLICE_BEGIN + i], opened.test(i));
            }
        );

        EXPECT_THROW(
            keys.open(io, SHA256::hash_to_128);
        , std::runtime_error);
        io.flush();
    }};

    senderThread.join();
    receiverThread.join();
}

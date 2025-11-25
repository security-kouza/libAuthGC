#include <gtest/gtest.h>

#include <iostream>
#include <thread>
#include <mutex>

#include <block_correlated_OT.hpp>


constexpr size_t OT_SIZE {128};
const std::string ADDRESS {"127.0.0.1"};
constexpr unsigned short PORT {12345};

namespace {

    void verify_bcot(const std::vector<emp::block>& localKeys,
                     const std::vector<emp::block>& macArr,
                     const ATLab::Bitset& choices,
                     const std::vector<emp::block>& deltaArr,
                     const size_t len) {
        for (size_t i = 0; i < deltaArr.size(); ++i) {
            for (size_t j = 0; j < len; ++j) {
                emp::block expected = localKeys[i * len + j];
                if (choices.test(j)) {
                    expected = expected ^ deltaArr[i];
                }
                ASSERT_EQ(ATLab::as_uint128(expected), ATLab::as_uint128(macArr[i * len + j]));
            }
        }
    }

} // namespace

TEST(BCOT, DEFAULT) {

    constexpr size_t deltaSize {3};

    std::vector<emp::block> deltaArr(deltaSize);
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& delta : deltaArr) {
        delta = ATLab::as_block(prng());
    }

    std::vector<emp::block> localKeys;
    ATLab::Bitset choices;
    std::vector<emp::block> macArr;

    std::thread bCOTSenderThread{
                    [&]() {
                        emp::NetIO io(emp::NetIO::SERVER, ADDRESS, PORT, true);
                        ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);
                        localKeys = sender.extend(OT_SIZE);
                    }
                }, bCOTReceiverThread{
                    [&]() {
                        emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, PORT, true);
                        ATLab::BlockCorrelatedOT::Receiver receiver(io, deltaSize);
                        auto [c, m] = receiver.extend(OT_SIZE);
                        choices = std::move(c);
                        macArr = std::move(m);
                    }
                };

    bCOTSenderThread.join();
    bCOTReceiverThread.join();

    ASSERT_EQ(localKeys.size(), deltaSize * OT_SIZE);
    ASSERT_EQ(macArr.size(), deltaSize * OT_SIZE);
    ASSERT_EQ(choices.size(), OT_SIZE);

    verify_bcot(localKeys, macArr, choices, deltaArr, OT_SIZE);
}

TEST(BCOT, MULTI_INSTANCE_REUSES_OT) {

    constexpr size_t firstDeltaSize {2};
    constexpr size_t secondDeltaSize {4};
    constexpr size_t firstLen {64};
    constexpr size_t secondLen {96};
    constexpr unsigned short ALT_PORT {static_cast<unsigned short>(PORT + 1)};

    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    std::vector<emp::block> deltaArr1(firstDeltaSize);
    for (auto& delta : deltaArr1) {
        delta = ATLab::as_block(prng());
    }
    std::vector<emp::block> deltaArr2(secondDeltaSize);
    for (auto& delta : deltaArr2) {
        delta = ATLab::as_block(prng());
    }

    std::vector<emp::block> keys1;
    std::vector<emp::block> keys2;
    std::vector<emp::block> macArr1;
    std::vector<emp::block> macArr2;
    ATLab::Bitset choices1;
    ATLab::Bitset choices2;

    std::thread senderThread{
        [&]() {
            emp::NetIO io(emp::NetIO::SERVER, ADDRESS, ALT_PORT, true);
            {
                auto senderDelta {deltaArr1};
                ATLab::BlockCorrelatedOT::Sender sender(io, std::move(senderDelta));
                keys1 = sender.extend(firstLen);
            }
            {
                ATLab::BlockCorrelatedOT::Receiver receiver(io, secondDeltaSize);
                auto [c, m] = receiver.extend(secondLen);
                choices2 = std::move(c);
                macArr2 = std::move(m);
            }
        }
    };

    std::thread receiverThread{
        [&]() {
            emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, ALT_PORT, true);
            {
                ATLab::BlockCorrelatedOT::Receiver receiver(io, firstDeltaSize);
                auto [c, m] = receiver.extend(firstLen);
                choices1 = std::move(c);
                macArr1 = std::move(m);
            }
            {
                auto senderDelta {deltaArr2};
                ATLab::BlockCorrelatedOT::Sender sender(io, std::move(senderDelta));
                keys2 = sender.extend(secondLen);
            }
        }
    };

    senderThread.join();
    receiverThread.join();

    ASSERT_EQ(keys1.size(), firstDeltaSize * firstLen);
    ASSERT_EQ(macArr1.size(), firstDeltaSize * firstLen);
    ASSERT_EQ(choices1.size(), firstLen);

    ASSERT_EQ(keys2.size(), secondDeltaSize * secondLen);
    ASSERT_EQ(macArr2.size(), secondDeltaSize * secondLen);
    ASSERT_EQ(choices2.size(), secondLen);

    verify_bcot(keys1, macArr1, choices1, deltaArr1, firstLen);
    verify_bcot(keys2, macArr2, choices2, deltaArr2, secondLen);
}

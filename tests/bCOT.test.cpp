#include <gtest/gtest.h>

#include <iostream>
#include <thread>
#include <mutex>

#include <block_correlated_OT.hpp>


constexpr size_t OT_SIZE {128};
const std::string ADDRESS {"127.0.0.1"};
constexpr unsigned short PORT {12345};

TEST(BCOT, DEFAULT) {

    constexpr size_t deltaSize {3};

    std::vector<emp::block> deltaArr(deltaSize);
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& delta : deltaArr) {
        delta = ATLab::as_block(prng());
    }

    std::vector<emp::block> localKeys;
    std::vector<bool> choices;
    std::vector<emp::block> macArr;
    std::mutex mtx;

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

    for (size_t i = 0; i < deltaSize; ++i) {
        for (size_t j = 0; j < OT_SIZE; ++j) {
            emp::block expected = localKeys[i * OT_SIZE + j];
            if (choices[j]) {
                expected = expected ^ deltaArr[i];
            }
            ASSERT_EQ(ATLab::as_uint128(expected), ATLab::as_uint128(macArr[i * OT_SIZE + j]));
        }
    }
}

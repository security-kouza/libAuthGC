#include <gtest/gtest.h>
#include <emp-ot/IKNP.h>

#include <iostream>
#include <array>
#include <thread>
#include <mutex>

#include <EndemicOT/EndemicOT.h>

#include "PRNG.hpp"
#include "utils.hpp"

constexpr size_t COT_SIZE {1280};
const std::string ADDRESS {"127.0.0.1"};
constexpr unsigned short PORT {12345};

TEST(IKNP, ManualContor) {

    std::array<emp::block, 128> k0 {}, k1 {}, k {};

    std::array<bool, 128> b {Revelio::random_bool_array<128>()};

    auto COTChoices {Revelio::random_bool_array<COT_SIZE>()};
//    std::array<bool, 1> COTChoices {false};
    std::array<emp::block, COT_SIZE> keys {}, keyBases {};
    __uint128_t delta;

    std::thread EOTSenderThread {[&k0, &k1, &keys, &COTChoices]() {
        auto rd {Revelio::PRNG_Kyber::get_PRNG_Kyber()};
        for (auto& rawBuf : k0) {
            auto& buf = *reinterpret_cast<__uint128_t*>(&rawBuf);
            buf = rd();
        }
        for (auto& rawBuf : k1) {
            auto& buf = *reinterpret_cast<__uint128_t*>(&rawBuf);
            buf = rd();
        }
        Revelio::Socket socket(ADDRESS, PORT);
        socket.accept();
        Revelio::EndemicOT::batched_send(socket, k0.data(), k1.data(), k0.size());
        emp::IKNP::Receiver receiver(k0, k1, true);
        receiver.gen_keys(socket, keys.data(), COTChoices.data(), keys.size());

    }}, EOTReceiverThread {[&b, &k, &keyBases, &delta]() {
        Revelio::Socket socket(ADDRESS, PORT);
        sleep(1);
        socket.connect();
        Revelio::EndemicOT::batched_receive(socket, k.data(), b.data(), k.size());
        emp::IKNP::Sender sender(k, b, true);

        sender.gen_keybases(socket, keyBases.data(), keyBases.size());
        *reinterpret_cast<emp::block*>(&delta) = sender.delta();
    }};

    EOTSenderThread.join();
    EOTReceiverThread.join();

    for (size_t i {0}; i != keys.size(); ++i) {
        const auto& key {Revelio::as_uint128(keys.at(i))};
        const auto& keyBase {Revelio::as_uint128(keyBases.at(i))};
        if (COTChoices.at(i)) {
            EXPECT_EQ(key, keyBase ^ delta);
        } else {
            EXPECT_EQ(key, keyBase);
        }
    }
}

TEST(IKNP, EOT_Factory) {
    std::array<bool, 128> choicesOT {Revelio::random_bool_array<128>()};
    auto choicesCOT {Revelio::random_bool_array<COT_SIZE>()};
    std::array<emp::block, COT_SIZE> keysCOT {}, keyBasesCOT {};
    __uint128_t delta;

    std::thread EOTSenderThread {
        [&keysCOT, &choicesCOT]() {
            Revelio::Socket socket(ADDRESS, PORT);
            socket.accept();
            auto receiver {emp::IKNP::receiver_by_EOT(socket, Revelio::PRNG_Kyber::get_PRNG_Kyber())};
            receiver.gen_keys(socket, keysCOT.data(), choicesCOT.data(), keysCOT.size());
        }
    }, EOTReceiverThread {
        [&choicesOT, &keyBasesCOT, &delta]() {
            Revelio::Socket socket(ADDRESS, PORT);
            socket.connect();

            auto sender {emp::IKNP::sender_by_EOT(socket, choicesOT)};
            sender.gen_keybases(socket, keyBasesCOT.data(), keyBasesCOT.size());
            *reinterpret_cast<emp::block*>(&delta) = sender.delta();
        }
    };

    EOTSenderThread.join();
    EOTReceiverThread.join();

    for (size_t i {0}; i != keysCOT.size(); ++i) {
        const auto& key {Revelio::as_uint128(keysCOT.at(i))};
        const auto& keyBase {Revelio::as_uint128(keyBasesCOT.at(i))};
        if (choicesCOT.at(i)) {
            EXPECT_EQ(key, keyBase ^ delta);
        } else {
            EXPECT_EQ(key, keyBase);
        }
    }
}

#include <sstream>
#include <fstream>
#include <gtest/gtest.h>
#include <boost/asio.hpp>

#include <emp-tool/utils/hash.h>

#include "socket.hpp"
#include "utils.hpp"

using namespace boost::asio;

const std::string ADDRESS {"127.0.0.1"};
constexpr unsigned short PORT {12341};

TEST(ATLabSocket, BasicTCP) {
    std::thread server {[]() {
        constexpr std::array<uint8_t, 4> msg {0, 1, 2, 4};
        ATLab::Socket socket {ADDRESS, PORT};
        socket.accept();
        socket.write(msg.data(), sizeof(msg));
        socket.close();
    }}, client {[]() {
        std::array<uint8_t, 4> buf {};
        ATLab::Socket socket {ADDRESS, PORT};
        socket.connect();
        socket.read(buf.data(), sizeof(buf));
        socket.close();

        constexpr std::array<uint8_t, 4> expected {0, 1, 2, 4}, wrong {0, 1, 2, 3};
        EXPECT_EQ(buf, expected);
        EXPECT_NE(buf, wrong);
    }};
    server.join();
    client.join();
}

TEST(ATLabSocket, hash) {
    // std::mutex serverFirst;
    // serverFirst.lock();
    constexpr std::array<uint8_t, 5> msg {0, 1, 2, 3, 4};
    std::array<std::byte, 32> clientChallenge {}, serverChallenge {};
    std::thread server {[&serverChallenge, &msg]() {
        std::array<uint8_t, 3> buf {};
        ATLab::Socket socket {ADDRESS, PORT};
        socket.accept();

        socket.write(msg.data(), 3);
        const auto firstThree {socket.gen_challenge()};
        socket.read(buf.data(), 2);
        const auto wholeMsg {socket.gen_challenge()};

        socket.write(firstThree.data(), firstThree.size());
        socket.write(wholeMsg.data(), wholeMsg.size());

        serverChallenge = socket.gen_challenge();

        socket.close();
    }}, client {[&clientChallenge, &msg]() {
        std::array<uint8_t, 3> buf {};
        std::array<std::byte, 32> serverFirstThree {}, serverWholeChallenge {}, empFirst {}, empWhole {};
        ATLab::Socket socket {ADDRESS, PORT};

        socket.connect();
        socket.read(buf.data(), 3);
        const auto localFirstThree {socket.gen_challenge()};
        socket.write(msg.data() + 3, 2);
        const auto localWholeChallenge {socket.gen_challenge()};

        socket.read(serverFirstThree.data(), 32);
        socket.read(serverWholeChallenge.data(), 32);

        clientChallenge = socket.gen_challenge();
        socket.close();

        EXPECT_NE(localFirstThree, localWholeChallenge);
        EXPECT_EQ(localFirstThree, serverFirstThree);
        EXPECT_EQ(localWholeChallenge, serverWholeChallenge);

        emp::Hash::hash_once(empFirst.data(), msg.data(), 3);
        emp::Hash::hash_once(empWhole.data(), msg.data(), 5);

        EXPECT_EQ(localFirstThree, empFirst);
        EXPECT_EQ(localWholeChallenge, empWhole);
    }};
    server.join();
    client.join();

    EXPECT_EQ(serverChallenge, clientChallenge);
}

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

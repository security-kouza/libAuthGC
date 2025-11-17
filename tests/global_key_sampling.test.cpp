#include <gtest/gtest.h>

#include <iostream>
#include <thread>
#include <mutex>

#include <block_correlated_OT.hpp>

#include "global_key_sampling.hpp"


const std::string ADDRESS {"127.0.0.1"};
constexpr unsigned short PORT {12345};

TEST(Global_Key_Sampling, DEFAULT) {

    emp::block deltaA, deltaB, alpha0, beta0;

    std::thread garblerThread{[&](){
        emp::NetIO io(emp::NetIO::SERVER, ADDRESS, PORT, true);
        EXPECT_NO_THROW(
            ATLab::GlobalKeySampling::Garbler garlber(io);
            deltaA = garlber.get_delta();
            alpha0 = garlber.get_alpha_0();
        );
    }}, evaluatorThread{[&]() {
        emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, PORT, true);
        EXPECT_NO_THROW(
            ATLab::GlobalKeySampling::Evaluator evaluator(io);
            deltaB = evaluator.get_delta();
            beta0 = evaluator.get_beta_0();
        );
    }};

    garblerThread.join();
    evaluatorThread.join();

    EXPECT_TRUE(ATLab::get_LSB(deltaA));

    emp::block prod;
    emp::gfmul(deltaA, deltaB, &prod);
    EXPECT_TRUE(ATLab::get_LSB(prod));

    EXPECT_EQ(ATLab::as_uint128(prod), ATLab::as_uint128(_mm_xor_si128(alpha0, beta0)));
}

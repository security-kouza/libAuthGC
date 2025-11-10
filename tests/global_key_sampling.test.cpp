#include <gtest/gtest.h>

#include <iostream>
#include <thread>
#include <mutex>

#include <block_correlated_OT.hpp>

#include "global_key_sampling.hpp"


const std::string ADDRESS {"127.0.0.1"};
constexpr unsigned short PORT {12345};

TEST(Global_Key_Sampling, DEFAULT) {

    std::thread garblerThread{[](){
        emp::NetIO io(emp::NetIO::SERVER, ADDRESS, PORT, true);
        ATLab::GlobalKeySampling::Garbler garlber(io);
    }}, evaluatorThread{[]() {
        emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, PORT, true);
        EXPECT_NO_THROW( ATLab::GlobalKeySampling::Evaluator evaluator(io) );
    }};

    garblerThread.join();
    evaluatorThread.join();
}

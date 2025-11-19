#include <gtest/gtest.h>

#include <iostream>
#include <thread>

#include <ATLab/preprocess.hpp>


const std::string ADDRESS {"127.0.0.1"};
constexpr unsigned short PORT {12345};

TEST(Preprocess, DEFAULT) {
    const auto circuit {ATLab::Circuit("circuits/bristol_format/sha-1.txt")};

    std::thread garblerThread{[&](){
        emp::NetIO io(emp::NetIO::SERVER, ADDRESS, PORT, true);
        auto garblerMatrix {ATLab::Garbler::preprocess(io, circuit)};
    }}, evaluatorThread{[&]() {
        emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, PORT, true);
        auto evaluatorMatrix {ATLab::Evaluator::preprocess(io, circuit)};
    }};

    garblerThread.join();
    evaluatorThread.join();
}

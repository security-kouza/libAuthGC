#include <gtest/gtest.h>

#include <iostream>
#include <thread>

#include <ATLab/benchmark.hpp>
#include <ATLab/preprocess.hpp>

#include "test-helper.hpp"
#include "ATLab/2PC_execution.hpp"

TEST(full_execution, DEFAULT) {
    // const std::string circuitFile {"circuits/bristol_format/adder_32bit.txt"};
    // const auto circuit {ATLab::Circuit("test/ands.txt")};
    const std::string circuitFile {"circuits/bristol_format/AES-non-expanded.txt"};

    ATLab::Bitset output;

    std::thread garblerThread{[&](){
        auto& io {server_io()};
        ATLab::Garbler::full_protocol(io, circuitFile, ATLab::Bitset{});
        io.flush();
    }}, evaluatorThread{[&]() {
        auto& io {client_io()};
        output =  ATLab::Evaluator::full_protocol(io, circuitFile, ATLab::Bitset{});
        io.flush();
    }};

    garblerThread.join();
    evaluatorThread.join();
}

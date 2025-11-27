#include <gtest/gtest.h>

#include <iostream>
#include <thread>

#include <ATLab/benchmark.hpp>
#include <ATLab/preprocess.hpp>

#include "test-helper.hpp"

TEST(Preprocess, DEFAULT) {
    const auto circuit {ATLab::Circuit("circuits/bristol_format/adder_32bit.txt")};
    // const auto circuit {ATLab::Circuit("test/ands.txt")};
    // const auto circuit {ATLab::Circuit("circuits/bristol_format/AES-non-expanded.txt")};
    std::unique_ptr<ATLab::Garbler::PreprocessedData> pGarblerPreData;
    std::unique_ptr<ATLab::Evaluator::PreprocessedData> pEvaluatorPreData;

    std::thread garblerThread{[&](){
        BENCHMARK_INIT;
        BENCHMARK_START;
        auto& io {server_io()};
        pGarblerPreData = std::make_unique<ATLab::Garbler::PreprocessedData>(
            ATLab::Garbler::preprocess(io, circuit)
        );
        io.flush();
        BENCHMARK_END(Garbler)
    }}, evaluatorThread{[&]() {
        BENCHMARK_INIT;
        BENCHMARK_START;
        auto& io {client_io()};
        pEvaluatorPreData = std::make_unique<ATLab::Evaluator::PreprocessedData>(
            ATLab::Evaluator::preprocess(io, circuit)
        );
        io.flush();
        BENCHMARK_END(Evaluator)
    }};

    garblerThread.join();
    evaluatorThread.join();

    test_ITMacBits({
        {&pGarblerPreData->masks, &pEvaluatorPreData->maskKeys},
        {&pEvaluatorPreData->masks, &pGarblerPreData->maskKeys},
        {&pGarblerPreData->beaverTripleShares, &pEvaluatorPreData->beaverTripleKeys},
        {&pEvaluatorPreData->beaverTripleShares, &pGarblerPreData->beaverTripleKeys},
    });

    for (const auto& gate : circuit.gates) {
        if (!gate.is_and()) {
            continue;
        }

        // beaver triple test
        EXPECT_EQ(
            (pGarblerPreData->masks[gate.in0] ^ pEvaluatorPreData->masks[gate.in0]) &
            (pGarblerPreData->masks[gate.in1] ^ pEvaluatorPreData->masks[gate.in1]),
            pGarblerPreData->beaverTripleShares[circuit.and_gate_order(gate)] ^ pEvaluatorPreData->beaverTripleShares[circuit.and_gate_order(gate)]
        ) << "Gate: " << gate.in0 << ' ' << gate.in1 << ' ' << gate.out << " index " << circuit.and_gate_order(gate) << '\n';
    }
}

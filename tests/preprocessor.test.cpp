#include <gtest/gtest.h>

#include <iostream>
#include <thread>

#include <ATLab/benchmark.hpp>
#include <ATLab/preprocess.hpp>

#include "test-helper.hpp"

TEST(Preprocess, DEFAULT) {
    const auto circuit {ATLab::Circuit("circuits/bristol_format/adder_32bit.txt")};
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

    // ensure the same global key
    ASSERT_EQ(pGarblerPreData->masks.global_key_size(), 1);
    ASSERT_EQ(pGarblerPreData->maskKeys.global_key_size(), 1);
    ASSERT_EQ(pGarblerPreData->beaverTripleShares.global_key_size(), 1);
    ASSERT_EQ(pGarblerPreData->beaverTripleKeys.global_key_size(), 1);

    ASSERT_EQ(pEvaluatorPreData->masks.global_key_size(), 1);
    ASSERT_EQ(pEvaluatorPreData->maskKeys.global_key_size(), 1);
    ASSERT_EQ(pEvaluatorPreData->beaverTripleShares.global_key_size(), 1);
    ASSERT_EQ(pEvaluatorPreData->beaverTripleKeys.global_key_size(), 1);
    ASSERT_EQ(
        ATLab::as_uint128(pGarblerPreData->maskKeys.get_global_key(0)),
        ATLab::as_uint128(pGarblerPreData->beaverTripleKeys.get_global_key(0))
    );
    ASSERT_EQ(
        ATLab::as_uint128(pEvaluatorPreData->maskKeys.get_global_key(0)),
        ATLab::as_uint128(pEvaluatorPreData->beaverTripleKeys.get_global_key(0))
    );

    ASSERT_EQ(pGarblerPreData->beaverTripleShares.size(), circuit.andGateSize);
    ASSERT_EQ(pEvaluatorPreData->beaverTripleShares.size(), circuit.andGateSize);
    ASSERT_EQ(pGarblerPreData->beaverTripleKeys.size(), circuit.andGateSize);
    ASSERT_EQ(pEvaluatorPreData->beaverTripleKeys.size(), circuit.andGateSize);

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

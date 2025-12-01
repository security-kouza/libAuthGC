#include <gtest/gtest.h>

#include <iostream>
#include <thread>

#include <ATLab/benchmark.hpp>
#include <ATLab/preprocess.hpp>

#include "test-helper.hpp"
#include "ATLab/2PC_execution.hpp"
#include "emp-tool/circuits/bit.h"

namespace {
    using namespace ATLab;

    Bitset full_execution_tester(
        const std::string& circuitFile,
        Bitset input0,
        Bitset input1
    ) {
        Bitset output;
        std::thread garblerThread{[&](){
            auto& io {server_io()};
            Garbler::full_protocol(io, circuitFile, std::move(input0));
            io.flush();
        }}, evaluatorThread{[&]() {
            auto& io {client_io()};
            output = Evaluator::full_protocol(io, circuitFile, std::move(input1));
            io.flush();
        }};

        garblerThread.join();
        evaluatorThread.join();

        return output;
    }

    template<class PreprocessedData>
    PreprocessedData gen_pre_data_zero(const Circuit& circuit) {
        static_assert(std::disjunction_v<
            std::is_same<PreprocessedData, Garbler::PreprocessedData>,
            std::is_same<PreprocessedData, Evaluator::PreprocessedData>
        >);

        const emp::block delta {emp::makeBlock(0, 1)};

        Bitset wireMasks(circuit.wireSize, 0);
        std::vector<emp::block> macs(circuit.wireSize, zero_block()), keys(circuit.wireSize, zero_block());

        Bitset beaverTriples(circuit.andGateSize, 0);
        std::vector<emp::block>
            beaverMacs(circuit.andGateSize, zero_block()),
            beaverKeys(circuit.andGateSize, zero_block());

        return {
            ITMacBits{std::move(wireMasks), std::move(macs)},
            ITMacBitKeys{std::move(keys), {delta}},
            ITMacBits{std::move(beaverTriples), std::move(beaverMacs)},
            ITMacBitKeys{std::move(beaverKeys), {delta}}
        };
    }

    Bitset zero_evaluate_test(const Circuit& circuit, const Evaluator::ReceivedGarbledCircuit& gc, Bitset inputs) {
        auto one_block {[]() {
            return _mm_set_epi64x(0, 1);
        }};

        std::vector<emp::block> labels(inputs.size(), zero_block());
        for (size_t i {0}; i != inputs.size(); ++i) {
            if (inputs[i]) {
                labels[i] = one_block();
            }
        }

        const auto garbledResults {Evaluator::evaluate(
            circuit,
            gen_pre_data_zero<Evaluator::PreprocessedData>(circuit),
            gc,
            std::move(labels),
            std::move(inputs)
        )};

        // return the last circuit.outputSize bits in garbledResults.maskedValues
        Bitset result(circuit.outputSize);
        for (size_t i = 0; i < circuit.outputSize; ++i) {
            result[i] = garbledResults.maskedValues[garbledResults.maskedValues.size() - circuit.outputSize + i];
        }
        return result;
    }


    template <size_t N>
    using TestType = std::array<std::pair<unsigned long, unsigned long>, N>;

    template <size_t N>
    void zero_tester(
        const std::string& circuitFile,
        const TestType<N>& tests
    ) {

        const Circuit circuit {circuitFile};

        GarbledTableVec garbledTables;
        Bitset wireMaskShift;
        Evaluator::ReceivedGarbledCircuit gc;

        std::thread garblerThread{[&](){
            auto& io {server_io()};
            Garbler::garble(
                io,
                circuit,
                gen_pre_data_zero<Garbler::PreprocessedData>(circuit),
                {circuit.totalInputSize, zero_block()}
            );

            io.flush();
        }}, evaluatorThread{[&]() {
            auto& io {client_io()};
            auto receivedGC {Evaluator::garble(io, circuit)};
            gc = std::move(receivedGC);
            io.flush();
        }};

        garblerThread.join();
        evaluatorThread.join();

        for (size_t i {0}; i < tests.size(); ++i) {
            EXPECT_EQ(
                zero_evaluate_test(circuit, gc, Bitset{circuit.totalInputSize, tests[i].first}).to_ulong(),
                static_cast<unsigned long>(tests[i].second)
            ) << ", where i = " << i << '\n';
        }
    }
}

using ATLab::Bitset;

TEST(full_execution, one_gate_AND) {
    // const std::string circuitFile {"circuits/bristol_format/adder_32bit.txt"};
    // const auto circuit {ATLab::Circuit("test/ands.txt")};
    const std::string circuitFile {"circuits/one-gate-AND.txt"};
    constexpr std::array<std::array<unsigned long, 3>, 1> truthTable {{
        // {0, 0, 0},
        // {0, 1, 0},
        // {1, 0, 0},
        {1, 1, 1},
    }};
    for (size_t i {0}; i < truthTable.size(); ++i) {
        EXPECT_EQ(
            full_execution_tester(
                circuitFile,
                Bitset{1, truthTable[i][0]},
                Bitset{1, truthTable[i][1]})
            .to_ulong(),
            truthTable[i][2]
        ) << ", where i = " << i << '\n';
    }

}

// Setting masks, Label0 to zero, global keys to 1
TEST(full_execution, zero_test) {
    zero_tester("circuits/one-gate-AND.txt", TestType<4> {{
        {0, 0},
        {1, 0},
        {2, 0},
        {3, 1}
    }});

    zero_tester("circuits/one-gate-XOR.txt", TestType<4>{{
        {0, 0},
        {1, 1},
        {2, 1},
        {3, 0}
    }});

    zero_tester("circuits/one-gate-NOT.txt", TestType<2> {{
        {0, 1},
        {1, 0}
    }});

    zero_tester("circuits/bristol_format/adder_32bit.txt", TestType<6>{{
        {(1ul << 32) + 1, 2},
        {(55ul << 32) + 196, 55 + 196},
        {(1283741ul << 32) + 19287387, 1283741 + 19287387},
        {0, 0},
        {static_cast<unsigned long>(-1), 0xFFFFFFFFul * 2},
        {(~0ul << 32) + 1, 1ul << 32}
    }});
}

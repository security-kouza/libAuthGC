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
    using namespace std::literals;

    struct TestVector {
        unsigned long input0, input1, output;
    };

    template <size_t N>
    using TestType = std::array<TestVector, N>;

    template <size_t N>
    void full_execution_tester(
        const std::string& circuitFile, TestType<N> tests
    ) {
        const Circuit circuit {circuitFile};
        std::array<Bitset, N> outputs;
        std::thread garblerThread{[&](){
            auto& io {server_io()};
            for (size_t i {0}; i != N; ++i) {
                Garbler::full_protocol(io, circuit, Bitset(circuit.inputSize0, tests[i].input0));
            }
            io.flush();
        }}, evaluatorThread{[&]() {
            auto& io {client_io()};
            for (size_t i {0}; i != N; ++i) {
                outputs[i] = Evaluator::full_protocol(io, circuit, Bitset(circuit.inputSize1, tests[i].input1));
            }
            io.flush();
        }};
        garblerThread.join();
        evaluatorThread.join();

        for (size_t i {0}; i != N; ++i) {
            EXPECT_EQ(outputs[i].to_ulong(), tests[i].output)
                << ", where i = " << i << " , circuit = " << circuitFile << '\n';
        }
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

    Bitset zero_evaluate_execute(
        const Circuit& circuit,
        const Evaluator::ReceivedGarbledCircuit& gc,
        Bitset input0,
        const Bitset& input1
    ) {
        auto one_block {[]() {
            return _mm_set_epi64x(0, 1);
        }};

        Bitset inputs {merge(std::move(input0), input1)};

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

    Evaluator::ReceivedGarbledCircuit  gen_zero_gc(const Circuit& circuit) {
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

        return gc;
    }

    template <size_t N>
    void zero_tester(
        const std::string& circuitFile,
        const TestType<N>& tests
    ) {
        const Circuit circuit {circuitFile};

        const auto gc {gen_zero_gc(circuit)};

        for (size_t i {0}; i < tests.size(); ++i) {
            EXPECT_EQ(
                zero_evaluate_execute(
                    circuit,
                    gc,
                    Bitset{circuit.inputSize0, tests[i].input0},
                    Bitset{circuit.inputSize1, tests[i].input1}
                ).to_ulong(),
                tests[i].output
            ) << ", where i = " << i << " of circuit " << circuitFile << '\n';
        }
    }

    struct TestVectorLarge {
        Bitset input0, input1, output;
    };

    // For inputs/outputs longer than 64 bits
    void zero_tester_large(
        const std::string& circuitFile,
        std::vector<TestVectorLarge> tests
    ) {
        const Circuit circuit {circuitFile};
        Evaluator::ReceivedGarbledCircuit gc {gen_zero_gc(circuit)};

        for (size_t i {0}; i < tests.size(); ++i) {
            EXPECT_EQ(
                zero_evaluate_execute(
                    circuit,
                    gc,
                    tests[i].input0,
                    tests[i].input1
                ),
                tests[i].output
            ) << ", where i = " << i << " of circuit " << circuitFile << '\n';
        }
    }

    constexpr TestType<4> andTests {{
        {0, 0, 0},
        {0, 1, 0},
        {1, 0, 0},
        {1, 1, 1}
    }}, xorTests {{
        {0, 0, 0},
        {0, 1, 1},
        {1, 0, 1},
        {1, 1, 0}
    }};
    constexpr TestType<2> notTests {{
        {0, 0, 1},
        {1, 0, 0}
    }};
    constexpr TestType<6> adderTests {{
        {0, 0, 0},
        {1, 1, 2},
        {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFFul * 2},
        {0xFFFFFFFF, 1, 1ul << 32},
        {1283741ul, 19287387, 1283741 + 19287387},
    }};
    const std::vector<TestVectorLarge> aesTest {{
        Bitset{128, 0},
        Bitset{128, 0},
        Bitset{
            "0111010011010100001011000101001110011010010111110011001000010001"
            "1101110000110100010100011111011100101011110100101001011101100110"s
        },
    }};
}

using ATLab::Bitset;

TEST(execution, full) {
    full_execution_tester("circuits/one-gate-AND.txt", andTests);
    full_execution_tester("circuits/one-gate-XOR.txt", xorTests);
    full_execution_tester("circuits/one-gate-NOT.txt", notTests);
    full_execution_tester("circuits/bristol_format/adder_32bit.txt", adderTests);
}

// Setting masks, Label0 to zero, global keys to 1
TEST(execution, zero_labels) {
    zero_tester("circuits/one-gate-AND.txt", andTests);
    zero_tester("circuits/one-gate-XOR.txt", xorTests);
    zero_tester("circuits/one-gate-NOT.txt", notTests);
    zero_tester("circuits/bristol_format/adder_32bit.txt", adderTests);
}

TEST(AES, zero_labels) {
    zero_tester_large("circuits/bristol_format/AES-non-expanded.txt", aesTest);
}

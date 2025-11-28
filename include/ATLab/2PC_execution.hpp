#ifndef ATLab_2PC_EXECUTION_HPP
#define ATLab_2PC_EXECUTION_HPP

#include <string>

#include "utils.hpp"
#include "circuit_parser.hpp"
#include "garble_evaluate.hpp"
#include "preprocess.hpp"

namespace ATLab {
    namespace Garbler {
        inline void full_protocol(emp::NetIO& io, const std::string& circuitFile, Bitset input) {
BENCHMARK_INIT;
BENCHMARK_START;
            Circuit circuit {circuitFile};
BENCHMARK_END(garbler circuit parser);
BENCHMARK_START;
            input.resize(circuit.inputSize0);
            const auto wireMasks {preprocess(io, circuit)};
BENCHMARK_END(garbler preprocessor);
BENCHMARK_START;
            const auto garbledCircuit {garble(io, circuit, wireMasks)};
BENCHMARK_END(gen garbled circuit);
BENCHMARK_START;

            // online

            Bitset maskedValues;
            maskedValues.reserve(circuit.inputSize0);
            for (Wire w {0}; w != circuit.inputSize0; ++w) {
                maskedValues.push_back(input[w] ^ wireMasks.masks[w]);
            }

            std::vector<BitsetBlock> rawMaskedValues {dump_raw_blocks(maskedValues)};
            io.send_data(rawMaskedValues.data(), rawMaskedValues.size() * sizeof(BitsetBlock));

            std::vector<emp::block> garblerInputLabels;
            garblerInputLabels.reserve(circuit.inputSize0);
            for (Wire w {0}; w != circuit.inputSize0; ++w) {
                garblerInputLabels.push_back(maskedValues[w] ? garbledCircuit.label1[w] : garbledCircuit.label0[w]);
            }
            io.send_data(garblerInputLabels.data(), garblerInputLabels.size() * sizeof(emp::block));

            BlockCorrelatedOT::OT& ot {BlockCorrelatedOT::Sender::Get_simple_OT(io.role)};
            ot.send(
                &garbledCircuit.label0[circuit.inputSize0],
                &garbledCircuit.label1[circuit.inputSize0],
                circuit.inputSize1
            );

            Bitset input1masks;
            input1masks.reserve(circuit.inputSize1);
            for (size_t i {circuit.inputSize0}; i != circuit.totalInputSize; ++i) {
                input1masks.push_back(wireMasks.masks[i]);
            }
            std::vector<BitsetBlock> rawInput1masks {dump_raw_blocks(input1masks)};
            io.send_data(rawInput1masks.data(), rawInput1masks.size() * sizeof(BitsetBlock));
            //TODO: macs

            Bitset outputMasks;
            outputMasks.reserve(circuit.outputSize);
            for (size_t i {0}; i != circuit.outputSize; ++i) {
                outputMasks.push_back(wireMasks.masks[circuit.wireSize - circuit.outputSize + i]);
            }
            std::vector<BitsetBlock> rawOutputMasks {dump_raw_blocks(outputMasks)};
            io.send_data(rawOutputMasks.data(), rawOutputMasks.size() * sizeof(BitsetBlock));
BENCHMARK_END(garbler online);
BENCHMARK_START;
        }
    }

    namespace Evaluator {
        inline Bitset full_protocol(emp::NetIO& io, const std::string& circuitFile, Bitset input) {
BENCHMARK_INIT
BENCHMARK_START;

            Circuit circuit {circuitFile};
BENCHMARK_END(evaluator circuit parser);
BENCHMARK_START;
            input.resize(circuit.inputSize1);
            const auto wireMasks {preprocess(io, circuit)};
BENCHMARK_END(evaluator preprocessor);
            const auto garbledCircuit {garble(io, circuit)};

BENCHMARK_START;
            // online
            std::vector<BitsetBlock> rawMaskedValues(calc_bitset_block(circuit.inputSize0));
            io.recv_data(rawMaskedValues.data(), rawMaskedValues.size() * sizeof(BitsetBlock));
            Bitset inputMaskedValues {rawMaskedValues.begin(), rawMaskedValues.end()};
            inputMaskedValues.resize(circuit.inputSize0);
            inputMaskedValues.reserve(circuit.wireSize);

            std::vector<emp::block> inputLabels(circuit.totalInputSize);
            io.recv_data(inputLabels.data(), circuit.inputSize0 * sizeof(emp::block));

            BlockCorrelatedOT::OT& ot {BlockCorrelatedOT::Receiver::Get_simple_OT(io.role)};
            auto* choices = new bool[circuit.inputSize1];
            for (size_t i {0}; i != circuit.inputSize1; ++i) {
                choices[i] = input[i];
            }
            ot.recv(inputLabels.data() + circuit.inputSize0, choices, circuit.inputSize1);

            std::vector<BitsetBlock> rawGarblerInput1Masks {calc_bitset_block(circuit.inputSize1)};
            io.recv_data(rawGarblerInput1Masks.data(), rawGarblerInput1Masks.size() * sizeof(BitsetBlock));
            Bitset garblerInput1Masks {rawGarblerInput1Masks.begin(), rawGarblerInput1Masks.end()};
            garblerInput1Masks.resize(circuit.inputSize1);
            for (size_t i {0}, j {circuit.inputSize0}; i != circuit.inputSize1; ++i, ++j) {
                inputMaskedValues.push_back(input[i] ^ wireMasks.masks[j] ^ garblerInput1Masks[i]);
            }

            const auto& [maskedValues, labels] {
                evaluate(circuit, wireMasks, garbledCircuit, std::move(inputLabels), std::move(inputMaskedValues))
            };

            // Get output masks
            std::vector<BitsetBlock> rawOutputMasks {calc_bitset_block(circuit.outputSize)};
            io.recv_data(rawOutputMasks.data(), rawOutputMasks.size() * sizeof(BitsetBlock));
            Bitset outputMasks {rawOutputMasks.begin(), rawOutputMasks.end()};
            outputMasks.resize(circuit.outputSize);

            Bitset output;
            output.reserve(circuit.outputSize);
            for (size_t i {0}; i != circuit.outputSize; ++i) {
                output.push_back(
                    maskedValues[circuit.wireSize - circuit.outputSize + i] ^
                    outputMasks[i] ^
                    wireMasks.masks[circuit.wireSize - circuit.outputSize + i]
                );
            }

            delete[] choices;
BENCHMARK_END(evaluator online);
            return output;
        }
    }
}

#endif // ATLab_2PC_EXECUTION_HPP

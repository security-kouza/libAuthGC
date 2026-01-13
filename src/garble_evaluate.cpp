#include <ATLab/garble_evaluate.hpp>

namespace ATLab {
    namespace Garbler {
        GarbledCircuit garble(
            ATLab::NetIO& io,
            const Circuit& circuit,
            const PreprocessedData& wireMasks,
            std::vector<emp::block> label0
        ) {
            const auto& [masks, maskKeys, beaverTriples, beaverTripleKeys] {wireMasks};
            const emp::block& globalKey {maskKeys.get_global_key(0)};

            // Initialize input wire labels
            std::vector<emp::block> label1(circuit.wireSize);
            if (label0.empty()) {
                // gen random labels
                label0.resize(circuit.wireSize);
                THE_GLOBAL_PRNG.random_block(label0.data(), circuit.totalInputSize);
            } else {
                // use passed label0
                assert(label0.size() == circuit.totalInputSize);
                label0.resize(circuit.wireSize);
            }
            for (size_t i {0}; i != circuit.totalInputSize; ++i) {
                label1[i] = _mm_xor_si128(label0[i], globalKey);
            }

            // garbled tables, for and gates
            GarbledTableVec garbledTables;
            garbledTables.reserve(circuit.andGateSize);
            Bitset wireMaskShift;
            wireMaskShift.reserve(circuit.andGateSize);

            // process output wires gate by gate
            for (size_t gateIter {0}, andGateIter {0}; gateIter != circuit.gateSize; ++gateIter) {
                switch (const auto& gate {circuit.gates[gateIter]}; gate.type) {
                case Gate::Type::NOT: {
                    label0[gate.out] = label1[gate.in0];
                    label1[gate.out] = label0[gate.in0];
                    break;
                }
                case Gate::Type::AND: {
                    emp::block tableEntry0 {_mm_xor_si128(
                        maskKeys.get_local_key(0, gate.in1),
                        and_all_bits(masks[gate.in1], globalKey)
                    )};
                    xor_to(tableEntry0, hash(label0[gate.in0], gate.out, 0));
                    xor_to(tableEntry0, hash(label1[gate.in0], gate.out, 0));

                    emp::block tableEntry1 {label0[gate.in0]};
                    xor_to(tableEntry1, and_all_bits(masks[gate.in0], globalKey));
                    xor_to(tableEntry1, maskKeys.get_local_key(0, gate.in0));
                    xor_to(tableEntry1, hash(label0[gate.in1], gate.out, 1));
                    xor_to(tableEntry1, hash(label1[gate.in1], gate.out, 1));

                    garbledTables.push_back({tableEntry0, tableEntry1});

                    label0[gate.out] = _mm_xor_si128(
                        hash(label0[gate.in0], gate.out, 0),
                        hash(label0[gate.in1], gate.out, 1)
                    );
                    xor_to(label0[gate.out], and_all_bits(
                               masks[gate.out] ^ beaverTriples[andGateIter],
                               globalKey
                           ));
                    xor_to(label0[gate.out], maskKeys.get_local_key(0, gate.out));
                    xor_to(label0[gate.out], beaverTripleKeys.get_local_key(0, andGateIter));

                    label1[gate.out] = _mm_xor_si128(label0[gate.out], globalKey);
                    wireMaskShift.push_back(get_LSB(label0[gate.out]));

                    ++andGateIter;
                    break;
                }
                case Gate::Type::XOR: {
                    label0[gate.out] = _mm_xor_si128(label0[gate.in0], label0[gate.in1]);
                    label1[gate.out] = _mm_xor_si128(label0[gate.out], globalKey);
                    break;
                }
                default: {
                    throw std::runtime_error{"Unexpected gate type"};
                }
                }
            }

            io.send_data(garbledTables.data(), garbledTables.size() * 2 * sizeof(emp::block));
            const auto rawWireMaskShift {dump_raw_blocks(wireMaskShift)};
            io.send_data(rawWireMaskShift.data(), rawWireMaskShift.size() * sizeof(BitsetBlock));

            return {std::move(label0), std::move(label1), std::move(garbledTables), std::move(wireMaskShift)};
        }
    }

    namespace Evaluator {
        ReceivedGarbledCircuit garble(ATLab::NetIO& io, const Circuit& circuit) {
            GarbledTableVec garbledTables(circuit.andGateSize, {zero_block(), zero_block()});

            io.recv_data(garbledTables.data(), circuit.andGateSize * 2 * sizeof(emp::block));
            std::vector<BitsetBlock> rawWireMaskShift(calc_bitset_block(circuit.andGateSize));
            io.recv_data(rawWireMaskShift.data(), rawWireMaskShift.size() * sizeof(BitsetBlock));
            Bitset wireMaskShift {rawWireMaskShift.begin(), rawWireMaskShift.end()};
            wireMaskShift.resize(circuit.andGateSize);

            return {std::move(garbledTables), std::move(wireMaskShift)};
        }

        EvaluateResult evaluate(
            const Circuit&                  circuit,
            const PreprocessedData&         wireMasks,
            const ReceivedGarbledCircuit&   garbledCircuit,
            std::vector<emp::block>         labels,
            Bitset                          maskedValues
        ) {
            const auto& masks {wireMasks.masks};
            const auto& beaverTriples {wireMasks.beaverTripleShares};
            const auto& garbledTables {garbledCircuit.garbledTables};
            const auto& wireMaskShift {garbledCircuit.wireMaskShift};

            maskedValues.resize(circuit.wireSize);
            labels.resize(circuit.wireSize);
            for (size_t gateIter {0}, andGateIter {0}; gateIter != circuit.gateSize; ++gateIter) {
                const auto& gate {circuit.gates[gateIter]};
                switch (gate.type) {
                case Gate::Type::NOT: {
                    maskedValues[gate.out] = not maskedValues[gate.in0];
                    labels[gate.out] = labels[gate.in0];
                    break;
                }
                case Gate::Type::AND: {
                    emp::block g0 {_mm_xor_si128(
                        garbledTables[andGateIter][0],
                        masks.get_mac(0, gate.in1)
                    )};
                    emp::block g1 {_mm_xor_si128(
                        garbledTables[andGateIter][1],
                        masks.get_mac(0, gate.in0)
                    )};
                    xor_to(g1, labels[gate.in0]);

                    // emp::block label {_mm_xor_si128(
                    //     hash(labels[gate.in0], gate.out, 0),
                    //     hash(labels[gate.in1], gate.out, 1)
                    // )};
                    const emp::block hash0 {hash(labels[gate.in0], gate.out, 0)};
                    const emp::block hash1 {hash(labels[gate.in1], gate.out, 1)};
                    emp::block label {_mm_xor_si128(hash0, hash1)};
                    xor_to(label, masks.get_mac(0, gate.out));
                    xor_to(label, beaverTriples.get_mac(0, andGateIter));
                    xor_to(label, and_all_bits(
                               maskedValues[gate.in0],
                               // _mm_xor_si128(g0, masks.get_mac(0, gate.in1))
                               g0
                           ));
                    xor_to(label, and_all_bits(
                               maskedValues[gate.in1],
                               // _mm_xor_si128(
                               g1
                                   // _mm_xor_si128(
                                   //     masks.get_mac(0, gate.in0),
                                   //     labels[gate.in0]
                                   // )
                               // )
                           ));
                    labels[gate.out] = label;
                    maskedValues[gate.out] = get_LSB(label) ^ wireMaskShift[andGateIter];

                    ++andGateIter;
                    break;
                }
                case Gate::Type::XOR: {
                    maskedValues[gate.out] = maskedValues[gate.in0] ^ maskedValues[gate.in1];
                    labels[gate.out] = _mm_xor_si128(labels[gate.in0], labels[gate.in1]);
                    break;
                }
                default: {
                    throw std::runtime_error{"Unexpected gate type"};
                }
                }
            }
            return {std::move(maskedValues), std::move(labels)};
        }
    }
}

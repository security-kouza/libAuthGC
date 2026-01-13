#include <array>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <emp-tool/utils/prg.h>

#include <ATLab/2PC_execution.hpp>
#include <params.hpp>
#include <utils.hpp>

namespace ATLab {
    namespace {
        constexpr size_t ChallengeBytes {
            (STATISTICAL_SECURITY + CHAR_BIT - 1) / CHAR_BIT
        };

        static_assert(STATISTICAL_SECURITY <= 64, "Only generating 64-bit challenge");

        emp::block sample_challenge_coeff(emp::PRG& prg) {
            return _mm_set_epi64x(0, static_cast<long long>(prg()));
        }

        emp::block calc_flip_mac_term(
            const Circuit& circuit,
            const Wire i,
            const Wire j,
            const emp::block& mac0,
            const emp::block mac1
        ) {
            emp::block sum {zero_block()};
            if (circuit.xor_source_list(i).test_flip()) {
                xor_to(sum, mac1);
            }
            if (circuit.xor_source_list(j).test_flip()) {
                xor_to(sum, mac0);
            }
            return sum;
        }
    }

    namespace Garbler {
        void check(
            ATLab::NetIO& io,
            const Circuit& circuit,
            const PreprocessedData& wireMasks,
            const GarbledCircuit& gc
        ) {
            uint64_t challenge;
            io.recv_data(&challenge, sizeof(challenge));
            const emp::block challengeSeed {_mm_set_epi64x(0, static_cast<long long>(challenge))};
            emp::PRG chalGen {&challengeSeed};

            std::vector<emp::block> coeff;
            coeff.reserve(circuit.andGateSize);
            for (size_t i {0}; i != circuit.andGateSize; ++i) {
                coeff.push_back(sample_challenge_coeff(chalGen));
            }

            emp::block accumulator {zero_block()};
            auto independent_wire_routine = [
                &circuit,
                &wireMasks,
                &coeff,
                &gc,
                &accumulator
            ](ATLab::NetIO& ioRef, const Wire w) {
                const auto& gcCheckData {circuit.gc_check_data(w)};
                emp::block ak1 {zero_block()};
                for (const auto& [gateIndex, connected] : gcCheckData) {
                    const auto& gate {circuit.gates[gateIndex]};
                    emp::block macTerm;
                    if (connected.to_ulong() == 3) {
                        macTerm = _mm_xor_si128(
                            wireMasks.masks.get_mac(0, gate.in0),
                            wireMasks.masks.get_mac(0, gate.in1)
                        );
                    } else if (connected[0]) {
                        macTerm = wireMasks.masks.get_mac(0, gate.in1);
                    } else {
                        macTerm = wireMasks.masks.get_mac(0, gate.in0);
                    }
                    emp::block prod;
                    emp::gfmul(macTerm, coeff[circuit.and_gate_order(gateIndex)], &prod);
                    xor_to(ak1, prod);
                }

                const emp::block ck {hash(gc.label0[w], w, 2)};
                xor_to(accumulator, ck);

                emp::block gk {ak1};
                xor_to(gk, ck);
                xor_to(gk, hash(gc.label1[w], w, 2));

                ioRef.send_data(&gk, sizeof(gk));
            };

            for (Wire w {0}; w != static_cast<Wire>(circuit.totalInputSize); ++w) {
                independent_wire_routine(io, w);
            }

            std::vector<emp::block> ak0Terms;
            ak0Terms.reserve(circuit.andGateSize);
            for (size_t gateIter {0}, andGateIter {0}; gateIter != circuit.gateSize; ++gateIter) {
                const auto& gate {circuit.gates[gateIter]};
                if (!gate.is_and()) {
                    continue;
                }

                independent_wire_routine(io, gate.out);

                emp::block ak0 {_mm_xor_si128(
                    wireMasks.masks.get_mac(0, gate.out),
                    wireMasks.beaverTripleShares.get_mac(0, andGateIter)
                )};

                xor_to(ak0, calc_flip_mac_term(
                    circuit,
                    gate.in0,
                    gate.in1,
                    wireMasks.masks.get_mac(0, gate.in0),
                    wireMasks.masks.get_mac(0, gate.in1)
                ));

                ak0Terms.push_back(ak0);
                ++andGateIter;
            }

            emp::block prodSum;
            emp::vector_inn_prdt_sum_red(&prodSum, ak0Terms.data(), coeff.data(), circuit.andGateSize);
            xor_to(accumulator, prodSum);

            io.send_data(&accumulator, ChallengeBytes);
        }
    }

    namespace Evaluator {
        void check(
            ATLab::NetIO& io,
            const Circuit& circuit,
            const PreprocessedData& wireMasks,
            const std::vector<emp::block>& labels,
            const Bitset& maskedValues
        ) {
            const auto& globalKey {wireMasks.maskKeys.get_global_key(0)};

            const uint64_t challenge {THE_GLOBAL_PRNG()};
            io.send_data(&challenge, sizeof(challenge));
            const emp::block challengeSeed {_mm_set_epi64x(0, static_cast<long long>(challenge))};
            emp::PRG chalGen {&challengeSeed};

            std::vector<emp::block> coeff;
            coeff.reserve(circuit.andGateSize);
            for (size_t i {0}; i != circuit.andGateSize; ++i) {
                coeff.push_back(sample_challenge_coeff(chalGen));
            }

            emp::block accumulator {zero_block()};
            auto independent_wire_routine = [&accumulator, &labels, &maskedValues](ATLab::NetIO& ioRef, const Wire w) {
                xor_to(accumulator, hash(labels[w], w, 2));
                emp::block received {zero_block()};
                ioRef.recv_data(&received, sizeof(received));
                if (maskedValues[w]) {
                    xor_to(accumulator, received);
                }
            };

            for (Wire w {0}; w != static_cast<Wire>(circuit.totalInputSize); ++w) {
                independent_wire_routine(io, w);
            }

            std::vector<emp::block> B;
            B.reserve(circuit.andGateSize);
            for (size_t gateIter {0}, andGateIter {0}; gateIter != circuit.gateSize; ++gateIter) {
                const auto& gate {circuit.gates[gateIter]};
                if (!gate.is_and()) {
                    continue;
                }

                independent_wire_routine(io, gate.out);

                const bool bPrime (
                    (maskedValues[gate.in0] && maskedValues[gate.in1]) ^
                    maskedValues[gate.out] ^
                    wireMasks.masks[gate.out] ^
                    wireMasks.beaverTripleShares[andGateIter] ^
                    (maskedValues[gate.in0] && wireMasks.masks[gate.in1]) ^
                    (maskedValues[gate.in1] && wireMasks.masks[gate.in0])
                );

                emp::block tmpB {and_all_bits(bPrime, globalKey)};
                xor_to(tmpB, wireMasks.maskKeys.get_local_key(0, gate.out));
                xor_to(tmpB, wireMasks.beaverTripleKeys.get_local_key(0, andGateIter));
                xor_to(tmpB,
                    and_all_bits(maskedValues[gate.in0], wireMasks.maskKeys.get_local_key(0, gate.in1))
                );
                xor_to(tmpB,
                    and_all_bits(maskedValues[gate.in1], wireMasks.maskKeys.get_local_key(0, gate.in0))
                );

                B.emplace_back(tmpB);
                ++andGateIter;
            }

            emp::block prodSum;
            emp::vector_inn_prdt_sum_red(&prodSum, coeff.data(), B.data(), circuit.andGateSize);
            xor_to(accumulator, prodSum);

            std::array<uint8_t, ChallengeBytes> ha {}, hb {};
            io.recv_data(ha.data(), ChallengeBytes);
            std::memcpy(hb.data(), &accumulator, ChallengeBytes);

            if (ha != hb) {
                throw std::runtime_error{"Malicious behavior detected: GC check failed"};
            }
        }
    }
}

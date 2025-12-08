#include "../include/ATLab/preprocess.hpp"

#include <unordered_map>
#include <algorithm>
#include <utility>

#include <ATLab/benchmark.hpp>
#include "global_key_sampling.hpp"

// TODO: n, L need names

namespace {
    using namespace ATLab;

    uint64_t pack(const uint32_t i, const uint32_t j) {
        return (static_cast<uint64_t>(i) << 32) | static_cast<uint64_t>(j);
    }

    std::pair<uint32_t, uint32_t> unpack(const uint64_t key) {
        auto i = static_cast<uint32_t>(key >> 32);
        auto j = static_cast<uint32_t>(key & 0xffffffffu);
        return {i, j};
    }

    class SparseBitMatrix {
        std::unordered_map<uint64_t, size_t> _map;
    public:
        explicit SparseBitMatrix(const size_t size) {
            // For `size` length bit string, each bit is 1 with probability 1/4
            // To reach best performance, setting the load factor to be 0.7
            // 1 / (4 * 0.7) is about 0.36
            _map.reserve(static_cast<size_t>(static_cast<double>(size) * 0.36));
        }

        void set(const uint32_t i, const uint32_t j, const size_t pos) {
            _map[pack(i, j)] = pos;
        }

        void reset(const uint32_t i, const uint32_t j) {
            _map.erase(pack(i, j));
        }

        bool test(const uint32_t i, const uint32_t j) const {
            return _map.find(pack(i, j)) != _map.end();
        }

        Bitset build_bitset(const size_t andGateSize) const {
            Bitset res(andGateSize, false);
            for (const auto& [key, pos] : _map) {
                res.set(pos);
            }
            return res;
        }
    };

    template <class T>
    class SparseStorage {
        std::unordered_map<uint64_t, T> _map;
    public:
        explicit SparseStorage(const size_t reserverSize) {
            _map.reserve(reserverSize);
        }

        void insert(const uint32_t i, const uint32_t j, T val) {
            _map[pack(i, j)] = std::move(val);
        }

        const T& at(const uint32_t i, const uint32_t j) const {
            return _map.at(pack(i, j));
        }

        const T& at(const typename decltype(_map)::iterator& it) {
            return _map.at(it->second);
        }

        const auto& end() {
            return _map.end();
        }

        auto try_emplace(const uint32_t i, const uint32_t j, T val = T{}) {
            return _map.try_emplace(pack(i, j), val);
        }

        const auto& find(const uint32_t i, const uint32_t j) {
            return _map.find(pack(i, j));
        }

        T release(const typename decltype(_map)::iterator& it) {
            return std::move(_map.extract(it).mapped());
        }
    };

    Matrix<bool> get_matrix(emp::NetIO& io, const size_t n, const size_t L) {
        const size_t blockSize {calc_matrix_blockSize(n, L)}; // TODO: Change to Total_block_count
        std::vector<MatrixBlock> rawData(blockSize);
        io.recv_data(rawData.data(), rawData.size() * sizeof(MatrixBlock));
        return {n, L, std::move(rawData)};
    }

    Matrix<bool> gen_and_send_matrix(emp::NetIO& io, const size_t n, const size_t L) {
        const size_t blockSize {calc_matrix_blockSize(n, L)};
        std::vector<MatrixBlock> rawData(blockSize);
        THE_GLOBAL_PRNG.random_data(rawData.data(), rawData.size() * sizeof(MatrixBlock));
        zero_matrix_row_padding(rawData, n, L);
        io.send_data(rawData.data(), rawData.size() * sizeof(MatrixBlock));
        return {n, L, std::move(rawData)};
    }

    struct PopulatedWireMasks {
        ITMacBits masks;
        ITMacBitKeys keys; // of the opponent's masks
        SparseBitMatrix andedMasks;
    };

    PopulatedWireMasks populate_wires_garbler(
        const Circuit& circuit,
        const ITMacBits& aMatrix,
        const ITMacBitKeys& bKeys,
        const size_t compressParam
    ) {
        Bitset masks(circuit.wireSize, false);
        std::vector<emp::block> macs(circuit.wireSize, _mm_set_epi64x(0, 0));
        std::vector<emp::block> evaluatorMaskKeys(circuit.wireSize, _mm_set_epi64x(0, 0));

        // Inputs : a is aMatrix last
        // Inputs 0: b is 0, so bKey is 0
        // Inputs 1: bKey is bKeys
        // AND outputs: a is aMatrix last, bKey is bKeys
        // XOR outputs: a is xor of inputs, bKey is xor of inputs
        // NOT outputs: a is input, bKey is input

        for (size_t wireIter {0}; wireIter != circuit.inputSize0; ++wireIter) {
            masks[wireIter] = aMatrix.at(wireIter);
            macs[wireIter] = aMatrix.get_mac(compressParam, wireIter);
        }
        size_t bKeysIter {0};
        for (size_t wireIter {circuit.inputSize0}; wireIter != circuit.totalInputSize; ++wireIter) {
            masks[wireIter] = aMatrix.at(wireIter);
            macs[wireIter] = aMatrix.get_mac(compressParam, wireIter);
            evaluatorMaskKeys[wireIter] = bKeys.get_local_key(0, bKeysIter);
            ++bKeysIter;
        }

        SparseBitMatrix andedMasks {circuit.andGateSize};
        size_t aMatrixIter {circuit.totalInputSize};
        for (const auto& gate : circuit.gates) {
            switch (gate.type) {
            case Gate::Type::NOT:
                masks[gate.out] = masks[gate.in0];
                macs[gate.out] = macs[gate.in0];
                evaluatorMaskKeys[gate.out] = evaluatorMaskKeys[gate.in0];
                break;

            case Gate::Type::AND:
                masks[gate.out] = aMatrix.at(aMatrixIter);
                macs[gate.out] = aMatrix.get_mac(compressParam, aMatrixIter);
                evaluatorMaskKeys[gate.out] = bKeys.get_local_key(0, bKeysIter);
                if (masks[gate.in0] && masks[gate.in1]) {
                    andedMasks.set(gate.in0, gate.in1, aMatrixIter - circuit.totalInputSize);
                }
                ++aMatrixIter;
                ++bKeysIter;
                break;

            case Gate::Type::XOR:
                masks[gate.out] = masks[gate.in0] ^ masks[gate.in1];
                macs[gate.out] = _mm_xor_si128(macs[gate.in0], macs[gate.in1]);
                evaluatorMaskKeys[gate.out] = _mm_xor_si128(evaluatorMaskKeys[gate.in0], evaluatorMaskKeys[gate.in1]);
                break;

            default:
                throw std::runtime_error{"Unexpected gate type"};
            }
        }

        return {
            ITMacBits{std::move(masks), std::move(macs)},
            ITMacBitKeys{std::move(evaluatorMaskKeys), {bKeys.get_global_key(0)}},
            std::move(andedMasks)
        };
    }

    PopulatedWireMasks populate_wires_evaluator(
        const Circuit& circuit,
        const ITMacBits& b,
        const ITMacBitKeys& aMatrix,
        const size_t compressParam
    ) {
        // Inputs : a.key is the next aMatrix
        // Inputs 0: b is 0, and b.mac is 0
        // Inputs 1: b is the next b
        // AND outputs: a.key is next aMatrix; b is the next b
        // XOR outputs: a.key is xor of inputs, b is xor of inputs
        // NOT outputs: a.key is input, b is input

        Bitset masks(circuit.wireSize, false);
        std::vector<emp::block> macs(circuit.wireSize, _mm_set_epi64x(0, 0));
        std::vector<emp::block> garblerMaskKeys(circuit.wireSize, _mm_set_epi64x(0, 0));

        for (size_t wireIter {0}; wireIter != circuit.inputSize0; ++wireIter) {
            garblerMaskKeys[wireIter] = aMatrix.get_local_key(compressParam, wireIter);
        }
        size_t bIter {0};
        for (size_t wireIter {circuit.inputSize0}; wireIter != circuit.totalInputSize; ++wireIter) {
            masks[wireIter] = b[bIter];
            macs[wireIter] = b.get_mac(0, bIter);
            garblerMaskKeys[wireIter] = aMatrix.get_local_key(compressParam, wireIter);
            ++bIter;
        }

        SparseBitMatrix andedMasks {circuit.andGateSize};
        size_t aMatrixIter {circuit.totalInputSize};
        for (const auto& gate : circuit.gates) {
            switch (gate.type) {
            case Gate::Type::NOT:
                masks[gate.out] = masks[gate.in0];
                macs[gate.out] = macs[gate.in0];
                garblerMaskKeys[gate.out] = garblerMaskKeys[gate.in0];
                break;

            case Gate::Type::AND:
                masks[gate.out] = b[bIter];
                macs[gate.out] = b.get_mac(0, bIter);
                garblerMaskKeys[gate.out] = aMatrix.get_local_key(compressParam, aMatrixIter);
                if (masks[gate.in0] && masks[gate.in1]) {
                    andedMasks.set(gate.in0, gate.in1, aMatrixIter - circuit.totalInputSize);
                }
                ++bIter;
                ++aMatrixIter;
                break;

            case Gate::Type::XOR:
                masks[gate.out] = masks[gate.in0] ^ masks[gate.in1];
                macs[gate.out] = _mm_xor_si128(macs[gate.in0], macs[gate.in1]);
                garblerMaskKeys[gate.out] = _mm_xor_si128(garblerMaskKeys[gate.in0], garblerMaskKeys[gate.in1]);
                break;

            default:
                throw std::runtime_error{"Unexpected gate type"};
            }
        }

        return {
            ITMacBits{std::move(masks), std::move(macs)},
            ITMacBitKeys{std::move(garblerMaskKeys), {aMatrix.get_global_key(compressParam)}},
            std::move(andedMasks)
        };
    }
}

namespace ATLab {

    // TODO: Separate circuit-independent and -dependent phases into two functions

    namespace Garbler {
        PreprocessedData preprocess(emp::NetIO& io, const Circuit& circuit) {
            const GlobalKeySampling::Garbler globalKey {io};

            const size_t evaluatorIndependentWireSize {circuit.andGateSize + circuit.inputSize1};
            const auto compressParam {static_cast<size_t>(calc_compression_parameter(evaluatorIndependentWireSize))};
            const auto independentWireSize {evaluatorIndependentWireSize + circuit.inputSize0};

            auto matrix {get_matrix(io, evaluatorIndependentWireSize, compressParam)};

            // 2
            const ITMacBitKeys bStarKeys {globalKey.get_COT_sender(), compressParam};
            auto bKeys {matrix * bStarKeys};

            // 3
            const ITMacBlockKeys dualAuthedBStar{io, globalKey.get_COT_sender(), compressParam};

            // 4
            BlockCorrelatedOT::Receiver sid1 {io, compressParam + 1};
            const ITMacBits aMatrix {sid1, independentWireSize};

            emp::block tmpDelta;
            THE_GLOBAL_PRNG.random_block(&tmpDelta, 1);
            const ITMacBlocks authedTmpDelta {io, sid1, {tmpDelta}};

            // 5
            BlockCorrelatedOT::Receiver sid2 {io, 2};
            ITMacBits beaverTripleShares {sid2, circuit.andGateSize};
            const ITMacBlocks authedTmpDeltaStep5 {io, sid2, {tmpDelta}};

            // 6
            auto [masks, maskKeys, andedMasks_nonconst] {populate_wires_garbler(circuit, aMatrix, bKeys, compressParam)};
            const auto& andedMasks {andedMasks_nonconst};

            // 7
            const ITMacBitKeys evaluatorAndedMasks {io, globalKey.get_COT_sender(), circuit.andGateSize};
            const ITMacBits    authedAndedMasks {io, sid2, andedMasks.build_bitset(circuit.andGateSize)};

BENCHMARK_INIT
BENCHMARK_START
            // 8
            const ITMacBlockKeys dualAuthedB {matrix * dualAuthedBStar};
            DualKeyAuthed_ab_Calculator ab {circuit, matrix, aMatrix, dualAuthedB};
BENCHMARK_END(G matrix * matrix);

BENCHMARK_START
            // 9
            std::vector<emp::block> tmpBeaverTriple(circuit.andGateSize, zero_block()); // \tilde{b_k}
            Bitset tmpBeaverTripleLsb(circuit.andGateSize);
            for (size_t gateIter {0}, andGateIndex {0}; gateIter != circuit.gateSize; ++gateIter) {
                const auto& gate {circuit.gates[gateIter]};
                if (!gate.is_and()) {
                    continue;
                }
                // AND gate

                // <a_i a_j> ^ <beaver triple share> ^ <a_i b_j> ^ <a_j b_i>
                emp::block& sum {tmpBeaverTriple[andGateIndex]};
                xor_to(sum, authedAndedMasks.get_mac(0, andGateIndex));
                xor_to(sum, beaverTripleShares.get_mac(0, andGateIndex));
                xor_to(sum, and_all_bits(
                    authedAndedMasks[andGateIndex] ^ beaverTripleShares[andGateIndex],
                    globalKey.get_alpha_0()
                ));

                xor_to(sum, ab(gate.in0, gate.in1));
                xor_to(sum, ab(gate.in1, gate.in0));

                tmpBeaverTripleLsb.set(andGateIndex, get_LSB(sum));
                ++andGateIndex;
            }

            std::vector<BitsetBlock> rawTmpBeaverTripleLsb {dump_raw_blocks(tmpBeaverTripleLsb)};
            io.send_data(rawTmpBeaverTripleLsb.data(), rawTmpBeaverTripleLsb.size() * sizeof(BitsetBlock));
BENCHMARK_END(G step 9);

            ITMacBitKeys beaverTripleKeys {io, globalKey.get_COT_sender(), circuit.andGateSize};

            return {
                std::move(masks),
                std::move(maskKeys),
                beaverTripleShares.extract_by_global_key(1),
                std::move(beaverTripleKeys)
            };
        }
    }

    namespace Evaluator {
        PreprocessedData preprocess(emp::NetIO& io, const Circuit& circuit) {
BENCHMARK_INIT;
            GlobalKeySampling::Evaluator globalKey {io};
            const auto evaluatorIndependentWireSize {circuit.andGateSize + circuit.inputSize1};
            const auto compressParam {static_cast<size_t>(calc_compression_parameter(evaluatorIndependentWireSize))};
            const auto independentWireSize {evaluatorIndependentWireSize + circuit.inputSize0};

            auto matrix {gen_and_send_matrix(io, evaluatorIndependentWireSize, compressParam)};

BENCHMARK_START;
            // 2
            const ITMacBits bStar {globalKey.get_COT_receiver(), compressParam};
            auto b {matrix * bStar};
BENCHMARK_END(E matrix multiplication);

            // 3
BENCHMARK_START;
            std::vector<emp::block> bStarDeltaB;
            bStarDeltaB.reserve(compressParam);
            for (size_t i {0}; i != compressParam; ++i) {
                bStarDeltaB.push_back(bStar[i] ? globalKey.get_delta() : _mm_set_epi64x(0, 0));
            }
            ITMacBlocks dualAuthedBStar{io, globalKey.get_COT_receiver(), std::move(bStarDeltaB)};
BENCHMARK_END(E step 3);

BENCHMARK_START;
            // 4
            std::vector<emp::block> sid1Keys {dualAuthedBStar.get_all_macs()};
            sid1Keys.push_back(globalKey.get_delta());
            BlockCorrelatedOT::Sender sid1 {io, std::move(sid1Keys)};
            const ITMacBitKeys aMatrix {sid1, independentWireSize};

            ITMacBlockKeys tmpDelta {io, sid1, 1};
BENCHMARK_END(E step 4); // Bottleneck

BENCHMARK_START
            // 5
            BlockCorrelatedOT::Sender sid2 {io, {globalKey.get_beta_0(), globalKey.get_delta()}};
            ITMacBitKeys beaverTripleKeys {sid2, circuit.andGateSize}; // keys to garbler's beaver triple shares
            ITMacBlockKeys tmpDeltaStep5 {io, sid2, 1};
BENCHMARK_END(E step 5);

BENCHMARK_START
            // 6
            auto [masks, maskKeys, andedMasks_nonconst] {populate_wires_evaluator(circuit, b, aMatrix, compressParam)};
            const auto& andedMasks {andedMasks_nonconst};
BENCHMARK_END(E step 6);

BENCHMARK_START
            const ITMacBits authedAndedMasks {
                io, globalKey.get_COT_receiver(), andedMasks.build_bitset(circuit.andGateSize)
            };
            const ITMacBitKeys garblerAndedMasks {io, sid2, circuit.andGateSize};
            io.flush();
BENCHMARK_END(E step 7);

BENCHMARK_START
            // 8
            // const auto dualAuthedB {matrix * dualAuthedBStar};
            DualKeyAuthed_ab_Calculator ab {circuit, matrix, aMatrix};
BENCHMARK_END(E step 8);

BENCHMARK_START
            // 9
            std::vector<emp::block> tmpBeaverTriple(circuit.andGateSize, zero_block()); // \tilde{b_k}
            Bitset tmpBeaverTripleLsb(circuit.andGateSize);
            for (size_t gateIter {0}, andGateIndex {0}; gateIter != circuit.gateSize; ++gateIter) {
                const auto& gate {circuit.gates[gateIter]};
                if (!gate.is_and()) {
                    continue;
                }
                // AND gate

                // <a_i a_j> ^ <beaver triple share> ^ <a_i b_j> ^ <a_j b_i>
                emp::block& sum {tmpBeaverTriple[andGateIndex]};
                xor_to(sum, garblerAndedMasks.get_local_key(0, andGateIndex));
                xor_to(sum, beaverTripleKeys.get_local_key(0, andGateIndex));

                xor_to(sum, ab(gate.in0, gate.in1));
                xor_to(sum, ab(gate.in1, gate.in0));

                tmpBeaverTripleLsb.set(andGateIndex, get_LSB(sum));
                ++andGateIndex;
            }

            std::vector<BitsetBlock> rawReceivedLsb(tmpBeaverTripleLsb.num_blocks());
            io.recv_data(rawReceivedLsb.data(), rawReceivedLsb.size() * sizeof(BitsetBlock));
            Bitset receivedLsb {rawReceivedLsb.begin(), rawReceivedLsb.end()};
            receivedLsb.resize(circuit.andGateSize);

            Bitset beaverTripleShare {receivedLsb ^ tmpBeaverTripleLsb};
            beaverTripleShare ^= authedAndedMasks.bits(); // \hat{b}_k = \tilde{b}_k ^ (b_i & b_j)
BENCHMARK_END(E step 9)

BENCHMARK_START
            ITMacBits authedBeaverTriple {io, globalKey.get_COT_receiver(), std::move(beaverTripleShare)};
BENCHMARK_END(E step 10)

            // DEBUG
            // std::clog << "key: ";
            // for (size_t i {0}, andGateIter {0}; andGateIter != 8; ++i) {
            //     const auto& gate {circuit.gates[i]};
            //     if (!gate.is_and()) {
            //         continue;
            //     }
            //     std::clog << get_LSB(dualAuthed_ab(gate.in0, gate.in1));
            //     ++andGateIter;
            // }
            // std::clog << '\n';

            return {
                std::move(masks),
                std::move(maskKeys),
                std::move(authedBeaverTriple),
                beaverTripleKeys.extract_by_global_key(1)
            };
        }
    }
}

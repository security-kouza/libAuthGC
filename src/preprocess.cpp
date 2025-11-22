#include "../include/ATLab/preprocess.hpp"

#include <sstream>
#include <boost/core/span.hpp>

#include <ATLab/benchmark.hpp>
#include "global_key_sampling.hpp"

// TODO: n, L need names

namespace {
    using namespace ATLab;

    Matrix<bool> get_matrix(emp::NetIO& io, const size_t n, const size_t L) {
        const size_t blockSize {calc_matrix_blockSize(n, L)};
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
}

namespace ATLab {
    namespace Garbler {
        PreprocessedData preprocess(emp::NetIO& io, const Circuit& circuit) {
            const GlobalKeySampling::Garbler globalKey {io};

            const auto n {circuit.andGateSize + circuit.inputSize1};
            const auto compressParam {static_cast<size_t>(calc_compression_parameter(n))};
            const auto m {n + circuit.inputSize0};

            auto matrix {get_matrix(io, n, compressParam)};

            // 2
            const ITMacBitKeys bStarKeys {globalKey.get_COT_sender(), compressParam};
            auto bKeys {matrix * bStarKeys};

            // 3
            const ITMacBlockKeys dualAuthedBStar{io, globalKey.get_COT_sender(), compressParam};

            // 4
            BlockCorrelatedOT::Receiver sid1 {io, compressParam + 1};
            ITMacBits aMatrix {sid1, m};

            emp::block tmpDelta;
            THE_GLOBAL_PRNG.random_block(&tmpDelta, 1);
            ITMacBlocks authedTmpDelta {io, sid1, {tmpDelta}};

            // 5
            BlockCorrelatedOT::Receiver sid2 {io, 2};
            const ITMacBits aHatMatrix {sid2, circuit.andGateSize};
            ITMacBlocks authedTmpDeltaStep5 {io, sid2, {tmpDelta}};


            return {std::move(matrix), std::move(bKeys)};
        }
    }

    namespace Evaluator {
        PreprocessedData preprocess(emp::NetIO& io, const Circuit& circuit) {
BENCHMARK_INIT;
            GlobalKeySampling::Evaluator globalKey {io};
            const auto n {circuit.andGateSize + circuit.inputSize1};
            const auto compressParam {static_cast<size_t>(calc_compression_parameter(n))};
            const auto m {n + circuit.inputSize0};

            auto matrix {gen_and_send_matrix(io, n, compressParam)};

            // 2
            const ITMacBits bStar {globalKey.get_COT_receiver(), compressParam};
            auto b {matrix * bStar};

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
            std::vector<emp::block> sid1Keys {std::move(dualAuthedBStar).release_macs()};
            sid1Keys.push_back(globalKey.get_delta());
            BlockCorrelatedOT::Sender sid1 {io, std::move(sid1Keys)};
            const ITMacBitKeys aMatrix {sid1, m};

            ITMacBlockKeys tmpDelta {io, sid1, 1};
BENCHMARK_END(E step 4);

BENCHMARK_START
            // 5
            BlockCorrelatedOT::Sender sid2 {io, {globalKey.get_beta_0(), globalKey.get_delta()}};
            ITMacBitKeys aHatMatrix {sid2, circuit.andGateSize};
            ITMacBlockKeys tmpDeltaStep5 {io, sid2, 1};
BENCHMARK_END(E step 5);

BENCHMARK_START
            // 6

BENCHMARK_END(E step 6);

            return {std::move(matrix), std::move(b)};
        }
    }
}

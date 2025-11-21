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
        Matrix<bool> preprocess(emp::NetIO& io, const Circuit& circuit) {
            const GlobalKeySampling::Garbler globalKey {io};

            const auto n {circuit.andGateSize + circuit.inputSize1};
            const auto compressParam {static_cast<size_t>(calc_compression_parameter(n))};
            const auto m {n + circuit.inputSize0};

            const auto matrix {get_matrix(io, n, compressParam)};

            // 2
            const ITMacBitKeys bStarKeys {globalKey.get_COT_sender(), compressParam};

BENCHMARK_INIT
BENCHMARK(Matrix multiplication | Garbler,
            const auto bKeys {matrix * bStarKeys};
);

            return matrix;
        }
    }

    namespace Evaluator {
        Matrix<bool> preprocess(emp::NetIO& io, const Circuit& circuit) {
BENCHMARK_INIT
BENCHMARK(Global key sampling time,
                GlobalKeySampling::Evaluator globalKey {io};
);
            const auto n {circuit.andGateSize + circuit.inputSize1};
            const auto compressParam {static_cast<size_t>(calc_compression_parameter(n))};
            const auto m {n + circuit.inputSize0};

BENCHMARK(Matrix generation time,
                const auto matrix {gen_and_send_matrix(io, n, compressParam)};
);

            // 2
            const ITMacBits bStar {globalKey.get_COT_receiver(), compressParam};

BENCHMARK(Matrix multiplication | Garbler,
            const auto b {matrix * bStar};
);

            return matrix;
        }
    }
}

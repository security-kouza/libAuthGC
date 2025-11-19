#include "../include/ATLab/preprocess.hpp"

#include <sstream>
#include <boost/core/span.hpp>

#include "global_key_sampling.hpp"

#define BENCHMARK(message, code) \
    start = std::chrono::high_resolution_clock::now();\
    code\
    end = std::chrono::high_resolution_clock::now();\
    std::cout << #message << ": "\
    << std::chrono::duration<double, std::milli>{end - start}.count() << "ms\n";

#define BENCHMARK_INIT std::chrono::time_point<std::chrono::system_clock> start, end;

// TODO: n, L need names

namespace {
    using namespace ATLab;

    Matrix<bool> get_matrix(emp::NetIO& io, const size_t n, const size_t L) {
        const size_t bitSize {n * L};

        const size_t blockSize {calc_bitset_blockSize(bitSize)};
        std::vector<BitsetBlock> rawData(blockSize);
        io.recv_data(rawData.data(), rawData.size());

        Bitset bitset {rawData.cbegin(), rawData.cend()};
        bitset.resize(bitSize);
        return {n, L, bitset};
    }

    Matrix<bool> gen_and_send_matrix(emp::NetIO& io, const size_t n, const size_t L) {
        const size_t bitSize {n * L};
        const size_t blockSize {calc_bitset_blockSize(bitSize)};
        std::vector<BitsetBlock> rawData(blockSize);
        THE_GLOBAL_PRNG.random_data(rawData.data(), rawData.size() * sizeof(BitsetBlock));

        io.send_data(rawData.data(), rawData.size());
        Bitset bitset (rawData.cbegin(), rawData.cend());
        bitset.resize(bitSize);
        return {n, L, std::move(bitset)};
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

            return matrix;
        }
    }

    namespace Evaluator {
        Matrix<bool> preprocess(emp::NetIO& io, const Circuit& circuit) {
            std::chrono::time_point<std::chrono::system_clock> start, end;
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

            return matrix;
        }
    }
}

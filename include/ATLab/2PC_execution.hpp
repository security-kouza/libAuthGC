#ifndef ATLab_2PC_EXECUTION_HPP
#define ATLab_2PC_EXECUTION_HPP

#include <string>

#include "utils.hpp"
#include "circuit_parser.hpp"
#include "garble_evaluate.hpp"
#include "preprocess.hpp"
#include "ATLab/benchmark.hpp"

namespace ATLab {
    namespace Garbler {
        // GCCheck
        void check(
            emp::NetIO& io,
            const Circuit& circuit,
            const PreprocessedData& wireMasks,
            const GarbledCircuit& gc
        );

        void online(
            emp::NetIO& io,
            const Circuit& circuit,
            const GarbledCircuit& gc,
            const PreprocessedData& wireMasks,
            const Bitset& input
        ) noexcept;

        inline void full_protocol(emp::NetIO& io, const Circuit& circuit, Bitset input) {
            input.resize(circuit.inputSize0);
            const auto wireMasks {preprocess(io, circuit)};
            const auto gc {garble(io, circuit, wireMasks)};

            online(io, circuit, gc, wireMasks, std::move(input));
        }

        inline void full_protocol(emp::NetIO& io, const std::string& circuitFile, const Bitset& input) {
            full_protocol(io, Circuit{circuitFile}, input);
        }
    }

    namespace Evaluator {
        // GCCheck
        void check(
            emp::NetIO& io,
            const Circuit& circuit,
            const PreprocessedData& wireMasks,
            const std::vector<emp::block>& labels,
            const Bitset& maskedValues
        );

        [[nodiscard]]
        Bitset online(
            emp::NetIO& io,
            const Circuit& circuit,
            const ReceivedGarbledCircuit& gc,
            const PreprocessedData& wireMasks,
            Bitset input
        );

        [[nodiscard]]
        inline Bitset full_protocol(emp::NetIO& io, const Circuit& circuit, Bitset input) {
BENCHMARK_INIT;
BENCHMARK_START;
            input.resize(circuit.inputSize1);
            const auto wireMasks {preprocess(io, circuit)};
BENCHMARK_END(evaluator preprocessor);
            const auto gc {garble(io, circuit)};
BENCHMARK_START;
            return online(io, circuit, gc, wireMasks, std::move(input));
BENCHMARK_END(evaluator online);
        }

        [[nodiscard]]
        inline Bitset full_protocol(emp::NetIO& io, const std::string& circuitFile, Bitset input) {
            return full_protocol(io, Circuit{circuitFile}, std::move(input));
        }
    }
}

#endif // ATLab_2PC_EXECUTION_HPP

#ifndef ATLab_GARBLE_HPP
#define ATLab_GARBLE_HPP

#include <circuit_parser.hpp>
#include <vector>

#include "global_key_sampling.hpp"
#include "preprocess.hpp"

namespace ATLab {
    inline emp::block hash(const emp::block& block, const Wire w, const int pad) {
        const std::array<emp::block, 2> blocks {block, _mm_set_epi64x(w, pad)};
        return emp::Hash::hash_for_block(blocks.data(), blocks.size() * sizeof(emp::block));
    }

    using GarbledTableVec = std::vector<std::array<emp::block, 2>>;

    namespace Garbler {
        struct GarbledCircuit {
            std::vector<emp::block> label0, label1;
            GarbledTableVec garbledTables;
            Bitset wireMaskShift;
        };

        GarbledCircuit garble(
            ATLab::NetIO& io,
            const Circuit& circuit,
            const PreprocessedData& wireMasks,
            std::vector<emp::block> label0 = {}
        );
    }

    namespace Evaluator {

        struct ReceivedGarbledCircuit {
            GarbledTableVec garbledTables;
            Bitset wireMaskShift;
        };

        ReceivedGarbledCircuit garble(ATLab::NetIO& io, const Circuit& circuit);

        struct EvaluateResult {
            Bitset maskedValues;
            std::vector<emp::block> labels;
        };

        EvaluateResult evaluate(
            const Circuit&                  circuit,
            const PreprocessedData&         wireMasks,
            const ReceivedGarbledCircuit&   garbledCircuit,
            std::vector<emp::block>         labels,
            Bitset                          maskedValues
        );
    }
}

#endif // ATLab_GARBLE_HPP

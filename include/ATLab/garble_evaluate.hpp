#ifndef ATLab_GARBLE_HPP
#define ATLab_GARBLE_HPP

#include <circuit_parser.hpp>

#include "global_key_sampling.hpp"

namespace ATLab {
    inline emp::block hash(const emp::block& block, const Wire w, const int pad) {
        const std::array<emp::block, 2> blocks {block, _mm_set_epi64x(w, pad)};
        return emp::Hash::hash_for_block(blocks.data(), blocks.size());
    }

    using GarbledTableVec = std::vector<std::array<emp::block, 2>>;

    namespace Garbler {
        struct GarbledCircuit {
            std::vector<emp::block> label0, label1;
            std::vector<std::array<emp::block, 2>> garbledTables;
            Bitset wireMaskShift;
        };
        GarbledCircuit garble(
            const Circuit&      circuit,
            const emp::block&   globalKey,
            const ITMacBits&    masks,
            const ITMacBitKeys& maskKeys,
            const ITMacBits&    beaverTriples,
            const ITMacBitKeys& beaverTripleKeys
        );
    }

    namespace Evaluator {
        struct EvaluateResult {
            Bitset maskedValues;
            std::vector<emp::block> labels;
        };

        EvaluateResult evaluate(
            const Circuit&          circuit,
            const GarbledTableVec&  garbledTables,
            const ITMacBits&        masks,
            const ITMacBits&        beaverTriples,
            Bitset                  maskedValues,
            std::vector<emp::block> labels,
            const Bitset&           wireMaskShift
        );
    }
}

#endif // ATLab_GARBLE_HPP

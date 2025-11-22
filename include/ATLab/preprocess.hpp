#ifndef ATLab_PREPROCESS_HPP
#define ATLab_PREPROCESS_HPP

#include <cassert>
#include <cmath>

#include <emp-tool/io/net_io_channel.h>

#include "../authed_bit.hpp"
#include "../circuit_parser.hpp"
#include "../params.hpp"

namespace ATLab {
    inline int calc_compression_parameter (size_t n) {
        static_assert(STATISTICAL_SECURITY == 40, "Only supporting ρ = 40.");
        constexpr double PreCalculatedConstant {-347.18};
        const auto res {
            std::ceil(std::log2(static_cast<double>(n)) * 2 * STATISTICAL_SECURITY + PreCalculatedConstant)
        };
#ifdef DEBUG
        assert(res > 0);
#endif // DEBUG
        return res;
    }

    // TODO: Separate circuit-independent and -dependent phases into two functions

    namespace Garbler {
        struct PreprocessedData {
            // TODO: names!!!
            Matrix<bool> matrix;
            ITMacBitKeys bKeys;
            // ITMacBits a;
            // ITMacBits aHat;
            // ITMacBitKeys bStar;
            // ITMacBitKeys bHat;
        };

        PreprocessedData preprocess(emp::NetIO&, const Circuit&);
    }

    namespace Evaluator {
        struct PreprocessedData {
            // TODO: names!!!
            Matrix<bool> matrix;
            ITMacBits b;
            // ITMacBitKeys a;
            // ITMacBitKeys aHat;
            // ITMacBits bStar;
            // ITMacBits bHat;
        };

        PreprocessedData preprocess(emp::NetIO&, const Circuit&);
    }
}

#endif // ATLab_PREPROCESS_HPP

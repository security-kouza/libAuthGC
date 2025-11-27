#ifndef ATLab_PREPROCESS_HPP
#define ATLab_PREPROCESS_HPP

#include <cassert>
#include <cmath>

#include <boost/core/span.hpp>

#include <emp-tool/io/net_io_channel.h>

#include "../authed_bit.hpp"
#include "../circuit_parser.hpp"
#include "../params.hpp"

namespace ATLab {
    inline int calc_compression_parameter (const size_t n) {
        static_assert(STATISTICAL_SECURITY == 40, "Only supporting ρ = 40.");
        constexpr double PreCalculatedConstant {-347.18};
        const auto res {
            std::ceil(std::log2(static_cast<double>(n)) * 2 * STATISTICAL_SECURITY + PreCalculatedConstant)
        };
        if (res < 1) {
            return 1;
        }
        return static_cast<int>(res);
    }

    // Calculate <a_i ^ b_j> when wires i and j are *independent* wires
    class DualKeyAuthed_ab_Calculator {
        std::vector<emp::block> _resFlatMatrix; // (i, j) stores <b_i ^ a_j>'s LSB

        const Circuit& _circuit;
        const size_t _totalIndependent;
    public:

        DualKeyAuthed_ab_Calculator(DualKeyAuthed_ab_Calculator&&) = default;

        // For garbler
        DualKeyAuthed_ab_Calculator(
            const Circuit& circuit,
            const Matrix<bool>& matrix,
            const ITMacBits& aMatrix,
            const ITMacBlockKeys& dualAuthedB
        ):
            _circuit {circuit},
            _totalIndependent {circuit.totalInputSize + circuit.andGateSize}
        {
            const size_t compressParam {matrix.colSize};

            // Transpose aMatrix's macs to easily create a span of columns
            const size_t&
                transposedRow {_totalIndependent},
                transposedCol {compressParam};
            std::vector<emp::block> transposedRawMacs; // global key major
            transposedRawMacs.reserve(transposedRow * transposedCol);

            // transpose rawMacs to _transposedRawMacs, ignoring the last row in rawMacs
            // rawMacs is a flatted matrix, with `compressParam` rows, `totalIndependent` columns
            for (size_t i = 0; i < transposedRow; ++i) {
                for (size_t j = 0; j < transposedCol; ++j) {
                    transposedRawMacs.push_back(aMatrix.get_mac(j, i));
                }
            }

            const size_t& resMatrixRow {matrix.rowSize}, resMatrixCol {_totalIndependent};
            _resFlatMatrix.reserve(resMatrixRow * resMatrixCol);

            // <b_i a_j>
            for (size_t row {0}; row != resMatrixRow; ++row) {
                const auto compressMatrixRow {matrix.row(row)};
                const emp::block alpha {dualAuthedB.get_local_key(0, row)};
                for (size_t col {0}; col != resMatrixCol; ++col) {
                    const boost::span<const emp::block> macVec {
                        transposedRawMacs.data() + col * transposedCol, transposedCol
                    };

                    emp::block mac {compressMatrixRow * macVec};

                    _resFlatMatrix.push_back(_mm_xor_si128(
                        compressMatrixRow * macVec,
                        and_all_bits(aMatrix[col], alpha)
                    ));
                }
            }
        }

        // For evaluator
        DualKeyAuthed_ab_Calculator(
            const Circuit& circuit,
            const Matrix<bool>& matrix,
            const ITMacBitKeys& aMatrix
        ):
            _circuit {circuit},
            _totalIndependent {circuit.totalInputSize + circuit.andGateSize}
        {
            const size_t compressParam {matrix.colSize};

            // Transpose aMatrix's keys to easily create a span of columns
            const size_t&
                transposedRow {_totalIndependent},
                transposedCol {compressParam};
            std::vector<emp::block> transposedRawKeys; // global key major
            transposedRawKeys.reserve(transposedRow * transposedCol);

            for (size_t i = 0; i < transposedRow; ++i) {
                for (size_t j = 0; j < transposedCol; ++j) {
                    transposedRawKeys.push_back(aMatrix.get_local_key(j, i));
                }
            }

            const size_t& resMatrixRow {matrix.rowSize}, resMatrixCol {_totalIndependent};
            _resFlatMatrix.reserve(resMatrixRow * resMatrixCol);

            // <b_i a_j>
            for (size_t row {0}; row != resMatrixRow; ++row) {
                const auto compressMatrixRow {matrix.row(row)};
                for (size_t col {0}; col != resMatrixCol; ++col) {
                    const boost::span<const emp::block> keyVec {
                        transposedRawKeys.data() + col * transposedCol, transposedCol
                    };
                    // only AND the LSB is enough
                    _resFlatMatrix.push_back(compressMatrixRow * keyVec);
                }
            }
        }

        /**
         * @param a, b must be independent wires
         */
        emp::block from_cache(const Wire a, const Wire b) const {
            if (b < static_cast<Wire>(_circuit.inputSize0)) {
                return zero_block();
            }

            size_t col {}, row {};
            if (a > static_cast<Wire>(_circuit.totalInputSize)) {
                col = _circuit.and_gate_order_by_output_wire(a) + _circuit.totalInputSize;
            } else {
                col = a;
            }
            if (b > static_cast<Wire>(_circuit.totalInputSize)) {
                row = _circuit.and_gate_order_by_output_wire(b) + _circuit.inputSize1;
            } else {
                row = b - _circuit.inputSize0;
            }

            // const size_t col {index_map(a)}, row {index_map(b)}; // <b_i a_j>
            return _resFlatMatrix.at(row * _totalIndependent + col);
        }

        emp::block operator()(const Wire in0, const Wire in1) const {
            emp::block res {_mm_set_epi64x(0, 0)};
            const auto& xorSource1 {_circuit.xor_source_list(in1)};
            _circuit.xor_source_list(in0).for_each_wire([this, &xorSource1, &res](const Wire i) {
                xorSource1.for_each_wire([this, &res, i](const Wire j) {
                    xor_to(res, from_cache(i, j));
                });
            });
            return res;
        }
    };

    namespace Garbler {
        struct PreprocessedData {
            ITMacBits masks;
            ITMacBitKeys maskKeys;
            ITMacBits beaverTripleShares;
            ITMacBitKeys beaverTripleKeys;
        };

        PreprocessedData preprocess(emp::NetIO&, const Circuit&);
    }

    namespace Evaluator {
        struct PreprocessedData {
            ITMacBits masks;
            ITMacBitKeys maskKeys;
            ITMacBits beaverTripleShares;
            ITMacBitKeys beaverTripleKeys;
        };

        PreprocessedData preprocess(emp::NetIO&, const Circuit&);
    }

}

#endif // ATLab_PREPROCESS_HPP

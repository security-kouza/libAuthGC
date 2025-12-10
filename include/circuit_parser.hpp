#ifndef ATLab_CIRCUIT_PARSER_HPP
#define ATLab_CIRCUIT_PARSER_HPP

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "utils.hpp"
#include "ATLab/matrix.hpp"


namespace ATLab {
	using Wire = int32_t;

	class XORSourceList {
		Matrix<bool>::RowView _positiveRow;
		Matrix<bool>::RowView _negativeRow;

		[[nodiscard]]
		size_t _block_count() const noexcept {
			return _positiveRow.block_count();
		}

		template <typename Callback>
		static void For_each_set_bit_(const Matrix<bool>::RowView& row, Callback&& callback) {
			row.for_each_set_bit([&](const size_t col) {
				callback(static_cast<Wire>(col));
			});
		}

	public:
		XORSourceList() = default;

		XORSourceList(
			Matrix<bool>::RowView positiveRow,
			Matrix<bool>::RowView negativeRow
		) noexcept:
			_positiveRow {std::move(positiveRow)},
			_negativeRow {std::move(negativeRow)}
		{}

		[[nodiscard]]
		const Matrix<bool>::RowView& positive_row() const & noexcept {
			return _positiveRow;
		}

		[[nodiscard]]
		const Matrix<bool>::RowView& negative_row() const & noexcept {
			return _negativeRow;
		}


		// Sequential
		template <typename Callback>
		void for_each_wire(Callback&& callback) const {
			const auto blockCount {_block_count()};
			if (!blockCount) {
				return;
			}
			const MatrixBlock* positiveRowData {_positiveRow.data()};
			const MatrixBlock* negativeRowData {_negativeRow.data()};
			if (!positiveRowData || !negativeRowData) {
				return;
			}
			const size_t colSize {_positiveRow.column_size()};
			for (size_t blockIter {0}; blockIter < blockCount; ++blockIter) {
				MatrixBlock mask {positiveRowData[blockIter] ^ negativeRowData[blockIter]};
				while (mask) {
					const size_t bitOffset {static_cast<size_t>(__builtin_ctzll(mask))};
					mask &= (mask - 1);
					const size_t col {blockIter * Matrix<bool>::bits_per_block + bitOffset};
					if (col >= colSize) {
						break;
					}
					callback(static_cast<Wire>(col));
				}
			}
		}

		// Sequential
		template <typename Callback>
		void for_each_positive_wire(Callback&& callback) const {
			For_each_set_bit_(_positiveRow, std::forward<Callback>(callback));
		}

		// Sequential
		template <typename Callback>
		void for_each_negative_wire(Callback&& callback) const {
			For_each_set_bit_(_negativeRow, std::forward<Callback>(callback));
		}

		bool has(const Wire wire) const {
#ifdef DEBUG
			if (wire < 0) {
				std::ostringstream sout;
				sout << "Accessing invalid wire " << wire << ".\n";
				throw std::invalid_argument{sout.str()};
			}
#endif // DEBUG
			const size_t column {static_cast<size_t>(wire)};
			return _positiveRow.test(column) || _negativeRow.test(column);
		}

		[[nodiscard]]
		bool empty() const noexcept {
			const auto blockCount {_block_count()};
			if (!blockCount) {
				return true;
			}
			const MatrixBlock* positiveRowData {_positiveRow.data()};
			const MatrixBlock* negativeRowData {_negativeRow.data()};
			if (!positiveRowData || !negativeRowData) {
				return true;
			}
			for (size_t blockIter {0}; blockIter < blockCount; ++blockIter) {
				if (positiveRowData[blockIter] || negativeRowData[blockIter]) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]]
		size_t size() const noexcept {
			return _positiveRow.column_size();
		}
	};

	class XORSourceMatrix {
		friend class Circuit;
		using MatrixBlock = Matrix<bool>::Block64;

		Matrix<bool> _positiveMatrix;
		Matrix<bool> _negativeMatrix;

		void _set_positive_bit(const size_t row, const size_t column) noexcept {
			assert(row < row_size() && column < col_size());

			const size_t blockIndex {column / Matrix<bool>::bits_per_block};
			const size_t bitOffset {column % Matrix<bool>::bits_per_block};
			MatrixBlock* rowData {_positiveMatrix.row_data(row)};
			rowData[blockIndex] |= (MatrixBlock{1} << bitOffset);
		}

		void _initialize(const size_t wireSize) {
			_positiveMatrix = Matrix<bool>{wireSize, wireSize};
			_negativeMatrix = Matrix<bool>{wireSize, wireSize};
		}

	public:
		XORSourceMatrix() = default;

		void set_as_independent_wire(const Wire wire) {
			const size_t row {static_cast<size_t>(wire)};
			assert(wire >= 0 && row < row_size());

			_set_positive_bit(row, row);
		}

		void assign_xor(const Wire out, const Wire in0, const Wire in1) {
			/**
			 * When both input wires have non-empty negative lists, XOR everything into the positive list.
			 * Otherwise, XOR the positive and negative lists separately.
			 */
			assert(out >= 0 && in0 >= 0 && in1 >= 0);

			const size_t rowOut {static_cast<size_t>(out)};
			const size_t rowIn0 {static_cast<size_t>(in0)};
			const size_t rowIn1 {static_cast<size_t>(in1)};
			const auto& rowSize {row_size()};
			assert(rowOut < rowSize && rowIn0 < rowSize && rowIn1 < rowSize);

			const size_t blockCount {_positiveMatrix.blocks_per_row()};
			MatrixBlock* outPos {_positiveMatrix.row_data(rowOut)};
			MatrixBlock* outNeg {_negativeMatrix.row_data(rowOut)};
			const MatrixBlock* in0Pos {_positiveMatrix.row_data(rowIn0)};
			const MatrixBlock* in1Pos {_positiveMatrix.row_data(rowIn1)};
			const MatrixBlock* in0Neg {_negativeMatrix.row_data(rowIn0)};
			const MatrixBlock* in1Neg {_negativeMatrix.row_data(rowIn1)};
			const auto in0NegRow {_negativeMatrix.row(rowIn0)};
			const auto in1NegRow {_negativeMatrix.row(rowIn1)};

			for (size_t i {0}; i < blockCount; ++i) {
				outPos[i] = in0Pos[i] ^ in1Pos[i];
			}

			if (!in0NegRow.empty() && !in1NegRow.empty()) {
				for (size_t i {0}; i < blockCount; ++i) {
					outPos[i] ^= in0Neg[i] ^ in1Neg[i];
				}
			} else if (in0NegRow.empty()) {
				std::memcpy(outNeg, in1Neg, blockCount * sizeof(MatrixBlock));
			} else if (in1NegRow.empty()) {
				std::memcpy(outNeg, in0Neg, blockCount * sizeof(MatrixBlock));
			}
			// If both empty, do nothing to outNeg to remain all 0
		}

		void assign_not(const Wire out, const Wire in) {
			/**
			 * When the input wire has a non-empty negative list, XOR the two lists into the positive list.
			 * Otherwise, out' negative list <- in's positive list
			 */
			const size_t outRow {static_cast<size_t>(out)};
			const size_t inRow {static_cast<size_t>(in)};
			const auto& rowSize {row_size()};
			assert(out >= 0 && in >= 0);
			assert(outRow < rowSize && inRow < rowSize);

			const size_t blockCount {_positiveMatrix.blocks_per_row()};
			MatrixBlock* outPos {_positiveMatrix.row_data(outRow)};
			MatrixBlock* outNeg {_negativeMatrix.row_data(outRow)};
			const MatrixBlock* inPos {_positiveMatrix.row_data(inRow)};
			const MatrixBlock* inNeg {_negativeMatrix.row_data(inRow)};

			if (const auto inNegRow {_negativeMatrix.row(inRow)}; inNegRow.empty()) {
				std::copy_n(inPos, blockCount, outNeg);
			} else {
				for (size_t i {0}; i < blockCount; ++i) {
					outPos[i] = inPos[i] ^ inNeg[i];
				}
			}
		}

		[[nodiscard]]
		XORSourceList row(const Wire wire) const noexcept {
			const size_t rowIndex {static_cast<size_t>(wire)};
#ifdef DEBUG
			assert(wire >= 0 && rowIndex < row_size());
#endif // DEBUG
			return XORSourceList{_positiveMatrix.row(rowIndex), _negativeMatrix.row(rowIndex)};
		}

		[[nodiscard]]
		size_t row_size() const noexcept {
			return _positiveMatrix.rowSize;
		}

		[[nodiscard]]
		size_t col_size() const noexcept {
			return _positiveMatrix.colSize;
		}
	};

	struct Gate {
		static constexpr Wire DISABLED {-1};

		enum class Type {
			AND, XOR, NOT
		};

		const Type type;
		const Wire in0, in1, out;
		const size_t index;

		Gate(const char typeInitLetter, const Wire inFirst, const Wire inSecond, const Wire output, const size_t index):
			type {(typeInitLetter == 'A') ? Type::AND : Type::XOR},
			in0 {inFirst},
			in1 {inSecond},
			out {output},
			index {index}
		{}

		Gate(const Wire input, const Wire output, const size_t index):
			type {Type::NOT},
			in0 {input},
			in1 {DISABLED},
			out {output},
			index {index}
		{}

		[[nodiscard]]
		bool is_and() const noexcept {
			return type == Type::AND;
		}
	};

	class Circuit {
		std::vector<size_t> _andGateOrder;
		std::vector<size_t> _andToGlobalIndex; // inverse of _andGateOrder
		XORSourceMatrix _xorSourceMatrix;
		std::unordered_map<Wire, size_t> _outputWireToGateIndex;

		// size: independent wires
		// bitset: [0] for i , [1] for j, set representing the wire is connected
		std::vector<std::unordered_map<size_t /*gate index*/, std::bitset<2> /*connected*/>> _gcCheckData;

		void _init_gc_check_data ();

	public:
		static constexpr size_t AND_ORDER_DISABLED {std::numeric_limits<size_t>::max()};

		size_t
			gateSize {},
			wireSize {},
			inputSize0 {},
			inputSize1 {},
			totalInputSize {},
			outputSize {},
			andGateSize {};

		std::vector<Gate> gates;

		explicit Circuit(const std::string& filename);

		[[nodiscard]]
		size_t and_gate_order(size_t gateIndex) const;

		[[nodiscard]]
		size_t and_gate_order(const Gate& gate) const {
			return and_gate_order(gate.index);
		}

		[[nodiscard]]
		XORSourceList xor_source_list(const Wire wire) const {
			return _xorSourceMatrix.row(wire);
		}


		[[nodiscard]]
		size_t gate_index_by_output_wire(const Wire outputWire) const {
			const auto it {_outputWireToGateIndex.find(outputWire)};
#ifdef DEBUG
			if (it == _outputWireToGateIndex.cend()) {
				std::ostringstream sout;
				sout << "Output wire " << outputWire << " not found.\n";
				throw std::out_of_range{sout.str()};
			}
#endif // DEBUG
			assert(it != _outputWireToGateIndex.cend());
			return it->second;
		}

		[[nodiscard]]
		const Gate& gate_by_output_wire(const Wire outputWire) const noexcept {
			assert(outputWire >= static_cast<Wire>(totalInputSize) && outputWire < static_cast<Wire>(wireSize));
			return gates[gate_index_by_output_wire(outputWire)];
		}

		[[nodiscard]]
		size_t and_gate_order_by_output_wire(const Wire outputWire) const {
			return and_gate_order(gate_index_by_output_wire(outputWire));
		}

		[[nodiscard]]
		size_t independent_index_map(const Wire w) const {
			if (w < static_cast<Wire>(totalInputSize)) {
				return w;
			}
			return and_gate_order_by_output_wire(w) + totalInputSize;
		}

		/**
		 * @param w wire index, NOT independent wire index
		 */
		[[nodiscard]]
		auto& gc_check_data(const Wire w) const {
			const size_t independentIndex {independent_index_map(w)};
			return _gcCheckData[independentIndex];
		}

		[[nodiscard]]
		size_t independent_size() const {
			return andGateSize + totalInputSize;
		}

		template <typename Callback>
		void for_each_AND_gate(Callback&& callback) const {
			for (size_t i {0}; i != _andToGlobalIndex.size(); ++i) {
				const size_t& andGateOrder {i};
				const Gate& gate {gates[_andToGlobalIndex[i]]};
				callback(gate, andGateOrder);
			}
		}

		[[nodiscard]]
		bool is_independent(const Wire w) const noexcept {
			assert(w >= 0 && w < static_cast<Wire>(wireSize));

			if (w < static_cast<Wire>(totalInputSize)) {
				return true;
			}
			return gates[gate_index_by_output_wire(w)].is_and();
		}
	};
}

#endif // ATLab_CIRCUIT_PARSER_HPP

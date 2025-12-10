#ifndef ATLab_CIRCUIT_PARSER_HPP
#define ATLab_CIRCUIT_PARSER_HPP

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <sys/stat.h>

#include "utils.hpp"
#include "ATLab/matrix.hpp"


namespace ATLab {
	using Wire = int32_t;

	class XORSourceList {
		Matrix<bool>::RowView _row;
		const bool _flip;

		[[nodiscard]]
		size_t _block_count() const noexcept {
			return _row.block_count();
		}

	public:
		XORSourceList() = delete;

		XORSourceList(
			Matrix<bool>::RowView row,
			const bool flip
		) noexcept:
			_row {std::move(row)},
			_flip {flip}
		{}

		[[nodiscard]]
		const Matrix<bool>::RowView& row() const & noexcept {
			return _row;
		}

		// Sequential
		template <class Func>
		void for_each_wire(Func&& fn) const {
			return _row.for_each_set_bit([func = std::forward<Func>(fn)](const size_t col) {
				func(static_cast<Wire>(col));
			});
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
			return _row.test(column);
		}

		[[nodiscard]]
		bool empty() const noexcept {
			return _row.empty();
		}

		[[nodiscard]]
		size_t size() const noexcept {
			return _row.column_size();
		}

		bool test_flip() const noexcept {
			return _flip;
		}
	};

	class XORSourceMatrix {
		friend class Circuit;
		using MatrixBlock = Matrix<bool>::Block64;

		Matrix<bool> _mat;
		Bitset _flip;

		void _set_positive_bit(const size_t row, const size_t column) noexcept {
			assert(row < row_size() && column < col_size());

			const size_t blockIndex {column / Matrix<bool>::bits_per_block};
			const size_t bitOffset {column % Matrix<bool>::bits_per_block};
			MatrixBlock* rowData {_mat.row_data(row)};
			rowData[blockIndex] |= (MatrixBlock{1} << bitOffset);
		}

		void _initialize(const size_t wireSize) {
			_mat = Matrix<bool>{wireSize, wireSize};
			_flip.resize(wireSize);
		}

		void _assign_xor(const Wire out, const Wire in0, const Wire in1) {
			assert(out >= 0 && in0 >= 0 && in1 >= 0);

			const auto wireSize {static_cast<Wire>(row_size())};
			assert(out < wireSize && in0 < wireSize && in1 < wireSize);

			const size_t blockCount {_mat.blocks_per_row()};
			MatrixBlock* outRow {_mat.row_data(out)};
			const MatrixBlock* in0Row {_mat.row_data(in0)};
			const MatrixBlock* in1Row {_mat.row_data(in1)};

			for (size_t i {0}; i < blockCount; ++i) {
				outRow[i] = in0Row[i] ^ in1Row[i];
			}
			_flip.set(out, _flip.test(in0) ^ _flip.test(in1));
		}

		void _assign_not(const Wire out, const Wire in) {
			/**
			 * When the input wire has a non-empty negative list, XOR the two lists into the positive list.
			 * Otherwise, out' negative list <- in's positive list
			 */
			const auto wireSize {static_cast<Wire>(row_size())};
			assert(out >= 0 && in >= 0);
			assert(out < wireSize && in < wireSize);

			const size_t blockCount {_mat.blocks_per_row()};
			MatrixBlock* outRow {_mat.row_data(out)};
			const MatrixBlock* inRow {_mat.row_data(in)};

			std::copy_n(inRow, blockCount, outRow);
			_flip.set(out, !_flip.test(in));
		}

	public:
		XORSourceMatrix() = default;

		void set_as_independent_wire(const Wire wire) {
			const size_t row {static_cast<size_t>(wire)};
			assert(wire >= 0 && row < row_size());

			_set_positive_bit(row, row);
		}

		[[nodiscard]]
		XORSourceList row(const Wire wire) const noexcept {
			assert(wire >= 0 && wire < static_cast<Wire>(row_size()));

			return XORSourceList{_mat.row(wire), _flip.test(wire)};
		}

		[[nodiscard]]
		size_t row_size() const noexcept {
			return _mat.rowSize;
		}

		[[nodiscard]]
		size_t col_size() const noexcept {
			return _mat.colSize;
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

		static void Populate_XOR_source_matrix_(
			const std::vector<Gate>& gates,
			XORSourceMatrix& xorSourceMatrix,
			const size_t totalInputSize,
			const size_t wireSize
		);

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

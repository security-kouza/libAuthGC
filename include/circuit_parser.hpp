#ifndef ATLab_CIRCUIT_PARSER_HPP
#define ATLab_CIRCUIT_PARSER_HPP

#include <limits>
#include <string>
#include <vector>

#include "utils.hpp"

namespace ATLab {
	using Wire = int32_t;

	class XORSourceList {
		/**
		 * all independent wires are in their own positive source.
		 * After an NOT gate, swap the positive and negative sources
		 * After an XOR gate, xor the sources respectively
		 */

		/**
		 * TODO: save only the independent wires, since Circuit::independent_index_map is implemented
		 */

		Bitset _sourcesPositive, _sourcesNegative;
		friend class Circuit;

	public:
		XORSourceList() = default;

		XORSourceList(const size_t wireSize, const size_t wireIndex):
			_sourcesPositive {wireSize},
			_sourcesNegative {wireSize}
		{
			_sourcesPositive.set(wireIndex);
		}

		XORSourceList(const XORSourceList& other) = default;
		XORSourceList(XORSourceList&& other) noexcept = default;
		XORSourceList& operator=(const XORSourceList& other) = default;
		XORSourceList& operator=(XORSourceList&& other) noexcept = default;

		// Sequential
		template <typename Callback>
		void for_each_wire(Callback&& callback) const {
			const Bitset fullSources {_sourcesNegative ^ _sourcesPositive};
			for (auto pos {fullSources.find_first()}; pos != Bitset::npos; pos = fullSources.find_next(pos)) {
				callback(static_cast<Wire>(pos));
			}
		}

		// Sequential
		template <typename Callback>
		void for_each_positive_wire(Callback&& callback) const {
			for (auto pos {_sourcesPositive.find_first()}; pos != Bitset::npos; pos = _sourcesPositive.find_next(pos)) {
				callback(static_cast<Wire>(pos));
			}
		}

		// Sequential
		template <typename Callback>
		void for_each_negative_wire(Callback&& callback) const {
			for (auto pos {_sourcesNegative.find_first()}; pos != Bitset::npos; pos = _sourcesNegative.find_next(pos)) {
				callback(static_cast<Wire>(pos));
			}
		}

		bool has(const Wire wire) const noexcept {
			return _sourcesPositive[wire] || _sourcesNegative[wire];
		}

		[[nodiscard]] bool empty() const noexcept {
			return _sourcesPositive.none() && _sourcesNegative.none();
		}

		[[nodiscard]] size_t size() const noexcept {
			return _sourcesPositive.size();
		}

		XORSourceList& operator^=(const XORSourceList& other) {
			_sourcesPositive ^= other._sourcesPositive;
			_sourcesNegative ^= other._sourcesNegative;
			return *this;
		}

		friend XORSourceList operator^(XORSourceList lhs, const XORSourceList& rhs) {
			lhs ^= rhs;
			return lhs;
		}

		friend XORSourceList operator~(XORSourceList list) {
			list._sourcesPositive ^= list._sourcesNegative;
			list._sourcesNegative ^= list._sourcesPositive;
			list._sourcesPositive ^= list._sourcesNegative;
			return list;
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

		[[nodiscard]] bool is_and() const noexcept {
			return type == Type::AND;
		}
	};

	class Circuit {
		std::vector<size_t> _andGateOrder;
		std::vector<size_t> _andToGlobalIndex; // inverse of _andGateOrder
		std::vector<std::unique_ptr<XORSourceList>> _pXORSourceListVec;
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
		const XORSourceList& xor_source_list(const Wire wire) const {
			return *_pXORSourceListVec[wire];
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
			return it->second;
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
			static size_t independentSize {andGateSize + totalInputSize};
			return independentSize;
		}
	};
}

#endif // ATLab_CIRCUIT_PARSER_HPP

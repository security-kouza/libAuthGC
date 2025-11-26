#ifndef ATLab_CIRCUIT_PARSER_HPP
#define ATLab_CIRCUIT_PARSER_HPP

#include <limits>
#include <string>
#include <vector>

#include "utils.hpp"

namespace ATLab {
	using Wire = int32_t;

	class Circuit;

	class XORSourceList {
		Bitset _sources;
		friend class Circuit;

	public:
		XORSourceList() = default;

		XORSourceList(const size_t wireSize, const size_t wireIndex):
			_sources {wireSize}
		{
			_sources.set(wireIndex);
		}

		XORSourceList(const XORSourceList& other) = default;
		XORSourceList(XORSourceList&& other) noexcept = default;
		XORSourceList& operator=(const XORSourceList& other) = default;
		XORSourceList& operator=(XORSourceList&& other) noexcept = default;

		template <typename Callback>
		void for_each_wire(Callback&& callback) const {
			for (auto pos {_sources.find_first()}; pos != Bitset::npos; pos = _sources.find_next(pos)) {
				callback(static_cast<Wire>(pos));
			}
		}

		[[nodiscard]] bool empty() const noexcept {
			return _sources.none();
		}

		[[nodiscard]] size_t size() const noexcept {
			return _sources.size();
		}

		XORSourceList& operator^=(const XORSourceList& other) {
			_sources ^= other._sources;
			return *this;
		}

		friend XORSourceList operator^(XORSourceList lhs, const XORSourceList& rhs) {
			lhs ^= rhs;
			return lhs;
		}
	};

	struct Gate {
		static constexpr Wire DISABLED {-1};

		enum class Type {
			AND, XOR, NOT
		};

		const Type type;
		const Wire in0, in1, out;

		Gate(const char typeInitLetter, const Wire inFirst, const Wire inSecond, const Wire output):
			type {(typeInitLetter == 'A') ? Type::AND : Type::XOR},
			in0 {inFirst},
			in1 {inSecond},
			out {output}
		{}

		Gate(const Wire input, const Wire output):
			type {Type::NOT},
			in0 {input},
			in1 {DISABLED},
			out {output}
		{}

		[[nodiscard]] bool is_and() const noexcept {
			return type == Type::AND;
		}
	};

	class Circuit {
		std::vector<size_t> _andGateOrder;
		std::vector<std::unique_ptr<XORSourceList>> _pXORSourceListVec;
	public:
		static constexpr size_t AND_ORDER_DISABLED {std::numeric_limits<size_t>::max()};

		size_t gateSize {}, wireSize {}, inputSize0 {}, inputSize1 {}, totalInputSize {}, outputSize {};
		size_t andGateSize {};
		std::vector<Gate> gates;

		explicit Circuit(const std::string& filename);

		[[nodiscard]]
		size_t and_gate_order(size_t gateIndex) const;

		[[nodiscard]]
		const XORSourceList& xor_source_list(const Wire wire) const {
			return *_pXORSourceListVec[wire];
		}
	};
}

#endif // ATLab_CIRCUIT_PARSER_HPP

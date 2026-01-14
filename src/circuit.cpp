#include <../include/ATLab/circuit_parser.hpp>

#include <sstream>
#include <fstream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_set>
#include <algorithm>

namespace ATLab {
	namespace {
#ifdef DEBUG
		void assert_no_redundant_not_gate(const Circuit& circuit) {
			// for each NOT gate, the input wire should have no NOT ancestor until independent wires

			// Mark all input wires checked.
			std::unordered_set<Wire> checkedWires;
			for (Wire w {0}; w < static_cast<Wire>(circuit.totalInputSize); ++w) {
				checkedWires.insert(w);
			}

			for (const auto& gate : circuit.gates) {
				if (gate.type != Gate::Type::NOT) {
					continue;
				}
				const Wire& in {gate.in0};
				/*
				 * To avoid recursion, maintain a queue of wires. Initialy only contains `in`.
				 * Loop until empty:
				 *   for the next wire popped from the queue,
				 *     if it's not checked, take its source gate:
				 *		 assert: The gate must not be a NOT gate.
				 *		 mark it checked
				 *		 if XOR: push the input wires into queue.
				 */
				std::queue<Wire> wiresToCheck;
				wiresToCheck.push(in);
				do {
					const Wire w {wiresToCheck.front()};
					wiresToCheck.pop();
					if (checkedWires.find(w) != checkedWires.end()) {
						continue;
					}
					const Gate& sourceGate {circuit.gates[circuit.gate_index_by_output_wire(w)]};
					if (sourceGate.type == Gate::Type::NOT) {
						throw std::invalid_argument{"The circuit contains redundant NOT gates."};
					}
					checkedWires.insert(w);
					if (sourceGate.type == Gate::Type::XOR) {
						wiresToCheck.push(sourceGate.in0);
						wiresToCheck.push(sourceGate.in1);
					}
				} while (!wiresToCheck.empty());
			}
		}
#endif // DEBUG

		void map_wires_order_gates (
			const std::vector<Gate>& gates,
			std::unordered_map<Wire, size_t>& outputWireToGateIndex,
			std::vector<size_t>& andGateOrder,
			std::vector<size_t>& andToGlobalIndex
		) {
			size_t currentAndOrder {0};
			for (size_t gateIter {0}; gateIter != gates.size(); ++gateIter) {
				const auto& gate {gates[gateIter]};
				outputWireToGateIndex[gate.out] = gateIter;

				if (gate.is_and()) {
					andGateOrder[gateIter] = currentAndOrder;
					andToGlobalIndex[currentAndOrder] = gateIter;
					++currentAndOrder;
				}
			}
		}
	}

	void Circuit::_init_gc_check_data() {
		_gcCheckData.resize(totalInputSize + andGateSize);
		for (size_t gateIter {0}; gateIter != gateSize; ++gateIter) {
			const auto& gate {gates[gateIter]};
			if (!gate.is_and()) {
				continue;
			}
			xor_source_list(gate.in0).for_each_wire([gateIter, this](const Wire w) {
				const size_t independentIndex {independent_index_map(w)};
				_gcCheckData[independentIndex][gateIter] = std::bitset<2>{1};
			});
			xor_source_list(gate.in1).for_each_wire([gateIter, this](const Wire w) {
				const size_t independentIndex {independent_index_map(w)};
				auto [it, inserted] {_gcCheckData[independentIndex].try_emplace(gateIter, 0)};
				it->second[1] = true;
			});
		}
	}

	void Circuit::Populate_XOR_source_matrix_(const std::vector<Gate>& gates, XORSourceMatrix& xorSourceMatrix,
		const size_t totalInputSize, const size_t wireSize) {
		const size_t inputInitLimit {std::min(totalInputSize, wireSize)};
		for (size_t wireIndex {0}; wireIndex != inputInitLimit; ++wireIndex) {
			xorSourceMatrix.set_as_independent_wire(static_cast<Wire>(wireIndex));
		}

		for (const auto& gate : gates) {
			switch (gate.type) {
			case Gate::Type::XOR:
				xorSourceMatrix._assign_xor(gate.out, gate.in0, gate.in1);
				break;
			case Gate::Type::AND:
				xorSourceMatrix.set_as_independent_wire(gate.out);
				break;
			case Gate::Type::NOT:
				xorSourceMatrix._assign_not(gate.out, gate.in0);
				break;
			}
		}
	}

	Circuit::Circuit(const std::string& filename) {
		std::ifstream fin {filename};
		if (!fin) {
			throw std::runtime_error{"Cannot open file " + filename};
		}

		fin >> gateSize >> wireSize;
		gates.reserve(gateSize);
		_andGateOrder.resize(gateSize, AND_ORDER_DISABLED);
		_xorSourceMatrix._initialize(wireSize);
		_outputWireToGateIndex.reserve(gateSize);

		fin >> inputSize0 >> inputSize1 >> outputSize;
		totalInputSize = inputSize0 + inputSize1;
		if (wireSize < totalInputSize) {
			throw std::runtime_error{"Circuit declares more inputs than wires"};
		}
		fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		for (size_t i {0}; i != gateSize; ++i) {
			char inputWireSize;
			fin >> inputWireSize;
			fin.ignore(3);
			if (inputWireSize == '2') {
				// AND or XOR gates
				Wire in0, in1, out;
				fin >> in0 >> in1 >> out;

				char gateInitLetter;
				fin >> gateInitLetter;
				fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

				if (gateInitLetter == 'A') {
					++andGateSize;
				}
				gates.emplace_back(gateInitLetter, in0, in1, out, i);
			} else {
				// NOT gates
				Wire in, out;
				fin >> in >> out;
				fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

				gates.emplace_back(in, out, i);
			}
		}
		_andToGlobalIndex.resize(andGateSize);

		map_wires_order_gates(gates, _outputWireToGateIndex, _andGateOrder, _andToGlobalIndex);

		Populate_XOR_source_matrix_(
			gates,
			_xorSourceMatrix,
			totalInputSize,
			wireSize
		);

		_init_gc_check_data();
	}

	size_t Circuit::and_gate_order(const size_t gateIndex) const {
#ifdef DEBUG
		if (gateIndex >= gateSize) {
			std::ostringstream sout;
			sout << "Accessing gate index " << gateIndex << ", but only " << gateSize << " gates exist.\n";
			throw std::out_of_range{sout.str()};
		}
		if (gates[gateIndex].type != Gate::Type::AND) {
			const std::string gateType {gates[gateIndex].type == Gate::Type::XOR ? "an XOR" : "a NOT"};
			std::ostringstream sout;
			sout << "Gate " << gateIndex << " is not an AND gate, but " << gateType << " gate.\n";
			throw std::runtime_error{sout.str()};
		}
#endif // DEBUG
		return _andGateOrder[gateIndex];
	}
}

#ifndef ATLab_CIRCUIT_PARSER_HPP
#define ATLab_CIRCUIT_PARSER_HPP

#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace ATLab {
	using Wire = int32_t;

	struct Gate {
		static constexpr Wire DISABLED {-1};
		const enum class Type {
			AND, XOR, NOT
		} type;
		const Wire in0, in1, out;

		// Ctor for XOR, AND
		Gate(const char typeInitLetter, const Wire inFirst, const Wire inSecond, const Wire output):
			type {(typeInitLetter == 'X') ? Type::XOR : Type::AND},
			in0 {inFirst},
			in1 {inSecond},
			out {output}
		{}

		// Ctor for NOT
		Gate(const Wire input, const Wire output):
			type {Type::XOR},
			in0 {input},
			in1 {DISABLED},
			out {output}
		{}
	};

	class Circuit {
		size_t _gateSize, _wireSize, _inputSize0, _inputSize1, _outputSize;
		std::vector<Gate> _gates;
	public:
		explicit Circuit(const std::string& filename) {
			std::ifstream fin {filename};
			if (!fin) {
				throw std::runtime_error("Cannot open file " + filename);
			}

			fin >> _gateSize >> _wireSize;
			_gates.reserve(gate_size());

			fin >> _inputSize0 >> _inputSize1 >> _outputSize;
			fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

			for (size_t i {0}; i != gate_size(); ++i) {
				if (fin.get() == '2') {
					// XOR or AND
					fin.ignore(3); // ignores " 1 "
					Wire in0, in1, out;
					fin >> in0 >> in1 >> out;

					char GateInitLetter;
					fin >> GateInitLetter;
					fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
					_gates.emplace_back(GateInitLetter, in0, in1, out);
				} else {
					// NOT
					fin.ignore(3); // ignores " 1 "
					Wire in, out;
					fin >> in >> out;
					fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

					_gates.emplace_back(in, out);
				}
			}
		}

		size_t gate_size() const {
			return _gateSize;
		}

		size_t wire_size() const {
			return _wireSize;
		}
	};
}

#endif // ATLab_CIRCUIT_PARSER_HPP

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
			type {(typeInitLetter == 'A') ? Type::AND : Type::XOR},
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

	struct Circuit {
		size_t gateSize, wireSize, inputSize0, inputSize1, outputSize;
		size_t andGateSize;
		std::vector<Gate> gates;
		explicit Circuit(const std::string& filename):
			andGateSize {0}
		{
			std::ifstream fin {filename};
			if (!fin) {
				throw std::runtime_error("Cannot open file " + filename);
			}

			fin >> gateSize >> wireSize;
			gates.reserve(gateSize);

			fin >> inputSize0 >> inputSize1 >> outputSize;
			fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

			for (size_t i {0}; i != gateSize; ++i) {
				if (fin.get() == '2') {
					// XOR or AND
					fin.ignore(3); // ignores " 1 "
					Wire in0, in1, out;
					fin >> in0 >> in1 >> out;

					char gateInitLetter;
					fin >> gateInitLetter;
					fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

					if (gateInitLetter == 'A') {
						++andGateSize;
					}
					gates.emplace_back(gateInitLetter, in0, in1, out);
				} else {
					// NOT
					fin.ignore(3); // ignores " 1 "
					Wire in, out;
					fin >> in >> out;
					fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

					gates.emplace_back(in, out);
				}
			}
		}
	};
}

#endif // ATLab_CIRCUIT_PARSER_HPP

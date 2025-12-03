#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include <emp-tool/io/net_io_channel.h>

#include <circuit_parser.hpp>
#include <ATLab/preprocess.hpp>
#include <ATLab/garble_evaluate.hpp>
#include <ATLab/2PC_execution.hpp>

#include <ATLab/benchmark.hpp>

#include "block_correlated_OT.hpp"

namespace po = boost::program_options;
using namespace ATLab;

namespace {
    template<class PreprocessedData>
    PreprocessedData gen_pre_data_zero(const ATLab::Circuit& circuit) {
        static_assert(std::disjunction_v<
            std::is_same<PreprocessedData, ATLab::Garbler::PreprocessedData>,
            std::is_same<PreprocessedData, ATLab::Evaluator::PreprocessedData>
        >);

        const emp::block delta {emp::makeBlock(0, 1)};

        ATLab::Bitset wireMasks(circuit.wireSize, 0);
        std::vector<emp::block>
            macs(circuit.wireSize, ATLab::zero_block()),
            keys(circuit.wireSize, ATLab::zero_block());

        ATLab::Bitset beaverTriples(circuit.andGateSize, 0);
        std::vector<emp::block>
            beaverMacs(circuit.andGateSize, ATLab::zero_block()),
            beaverKeys(circuit.andGateSize, ATLab::zero_block());

        return {
            ITMacBits{std::move(wireMasks), std::move(macs)},
            ITMacBitKeys{std::move(keys), {delta}},
            ITMacBits{std::move(beaverTriples), std::move(beaverMacs)},
            ITMacBitKeys{std::move(beaverKeys), {delta}}
        };
    }
}

void garbler(const Circuit& circuit, const std::string& host, const unsigned short port, const size_t iteration) {
    emp::NetIO io {emp::NetIO::SERVER, host, port, false};
    const auto zeroMasks {gen_pre_data_zero<Garbler::PreprocessedData>(circuit)};
    const auto gc {Garbler::garble(
        io,
        circuit,
        zeroMasks,
        {circuit.totalInputSize, ATLab::zero_block()}
    )};

BENCHMARK_INIT;
BENCHMARK_START;
    BlockCorrelatedOT::Sender::Initialize_simple_OT(io);
    for (size_t i {0}; i != iteration; ++i) {
        Garbler::online(io, circuit, gc, zeroMasks, Bitset{circuit.inputSize0, 0});
    }
BENCHMARK_END_ITERATION(garbler online, iteration)
}

void evaluator(const Circuit& circuit, const std::string& host, const unsigned short port, const size_t iteration) {
    emp::NetIO io {emp::NetIO::CLIENT, host, port, false};
    const auto zeroMasks {gen_pre_data_zero<Evaluator::PreprocessedData>(circuit)};
    const auto gc {Evaluator::garble(io, circuit)};

BENCHMARK_INIT;
BENCHMARK_START;
    BlockCorrelatedOT::Receiver::Initialize_simple_OT(io);
    for (size_t i {0}; i != iteration; ++i) {
        Evaluator::online(io, circuit, gc, zeroMasks, Bitset{circuit.inputSize1, 0});
    }
BENCHMARK_END_ITERATION(evaluator online, iteration);
}

int main(int argc, char* argv[]) {
    std::string phase, host, role, circuitFile;
    unsigned short port;
    unsigned int iteration;

    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Show this help message")
        ("phase", po::value(&phase)->default_value("online"), "Execution phase: online|all")
        ("role,r", po::value(&role), "garbler|evaluator")
        ("host", po::value(&host)->default_value("127.0.0.1"), "Garbler's listening IPv4 address")
        ("port,p", po::value(&port)->default_value(12345), "port")
        ("circuit,c", po::value(&circuitFile), "Path of the circuit file")
        ("iteration,i", po::value(&iteration)->default_value(128), "Number of iterations");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    if (vm.count("circuit") == 0) {
        std::cerr << "No circuit file specified. Aborting...\n";
        return 1;
    }
    if (vm.count("role") == 0) {
        std::cerr << "No role specified. Aborting...\n";
        return 1;
    }

    if (role != "garbler" && role != "evaluator") {
        std::cerr << "Invalid role. Aborting...\n";
        return 1;
    }
    if (phase != "online") {
        std::cerr << "Only supporting online phase\n";
        return 1;
    }

    const Circuit circuit {circuitFile};

    if (role == "garbler") {
        garbler(circuit, host, port, iteration);
    } else {
        evaluator(circuit, host, port, iteration);
    }

    return 0;
}

#include <gtest/gtest.h>

#include <../include/ATLab/circuit_parser.hpp>

#include <vector>

namespace {
    std::vector<ATLab::Wire> collectWires(const ATLab::XORSourceList& list) {
        std::vector<ATLab::Wire> wires;
        list.for_each_wire([&](const ATLab::Wire wire) {
            wires.push_back(wire);
        });
        return wires;
    }
}

TEST(Circuit_Parser, default) {
    EXPECT_NO_THROW(
        const auto circuit {ATLab::Circuit("circuits/bristol_format/adder_32bit.txt")};
    );
}

TEST(Circuit_Parser, populates_metadata_for_simple_circuit) {
    /*
        7 10
        2 1 1

        2 1 0 1 3 XOR
        2 1 1 2 4 XOR
        2 1 3 4 5 XOR
        2 1 3 4 6 AND
        2 1 5 6 7 XOR
        2 1 6 7 8 AND
        1 1 8 9 INV
    */
    const ATLab::Circuit circuit {"circuits/test_circuit.txt"};

    ASSERT_EQ(circuit.gateSize, 7);
    ASSERT_EQ(circuit.wireSize, 10);
    ASSERT_EQ(circuit.gates.size(), circuit.gateSize);
    EXPECT_EQ(circuit.andGateSize, 2);

    // Independent wires
    const auto wire0 {collectWires(circuit.xor_source_list(0))};
    const auto wire1 {collectWires(circuit.xor_source_list(1))};
    const auto wire6 {collectWires(circuit.xor_source_list(6))};

    EXPECT_EQ(wire0, (std::vector<ATLab::Wire>{0}));
    EXPECT_EQ(wire1, (std::vector<ATLab::Wire>{1}));
    EXPECT_EQ(wire6, (std::vector<ATLab::Wire>{6}));

    // Dependent wires
    const auto wire3 {collectWires(circuit.xor_source_list(3))};
    const auto wire4 {collectWires(circuit.xor_source_list(4))};
    const auto wire5 {collectWires(circuit.xor_source_list(5))};
    const auto wire9 {collectWires(circuit.xor_source_list(9))};

    EXPECT_EQ(wire3, (std::vector<ATLab::Wire>{0, 1}));
    EXPECT_EQ(wire4, (std::vector<ATLab::Wire>{1, 2}));
    EXPECT_EQ(wire5, (std::vector<ATLab::Wire>{0, 2}));
    EXPECT_EQ(wire9, (std::vector<ATLab::Wire>{8}));

    // AND order
    EXPECT_TRUE(circuit.gates[3].is_and());
    EXPECT_FALSE(circuit.gates[4].is_and());
    EXPECT_TRUE(circuit.gates[5].is_and());

    EXPECT_EQ(circuit.and_gate_order(3), 0);
    EXPECT_EQ(circuit.and_gate_order(5), 1);
#ifdef DEBUG
    EXPECT_THROW(auto res {circuit.and_gate_order(4)}, std::exception);
#endif // DEBUG
}

TEST(Circuit_Parser, gc_check_data) {
    /*
        7 10
        2 1 1

        2 1 0 1 3 XOR
        2 1 1 2 4 XOR
        2 1 3 4 5 XOR
        2 1 3 4 6 AND
        2 1 5 6 7 XOR
        2 1 6 7 8 AND
        1 1 8 9 INV
    */
    const ATLab::Circuit circuit {"circuits/test_circuit.txt"};

    auto test_independent_wire {[&circuit](const ATLab::Wire w) {
        const auto& gcCheckData {circuit.gc_check_data(w)};
        for (const auto& [gateIndex, connected] : gcCheckData) {
            const auto& gate {circuit.gates[gateIndex]};

            EXPECT_NE(connected.to_ulong(), 0);
            if (connected.test(0)) {
                EXPECT_TRUE(circuit.xor_source_list(gate.in0).has(w));
            }
            if (connected.test(1)) {
                EXPECT_TRUE(circuit.xor_source_list(gate.in1).has(w));
            }
        }
    }};

    for (ATLab::Wire w {0}; w != static_cast<ATLab::Wire>(circuit.totalInputSize); ++w) {
        test_independent_wire(w);
    }

    for (size_t i {0}; i != circuit.gateSize; ++i) {
        const auto& gate {circuit.gates[i]};
        if (!gate.is_and()) {
            continue;
        }
        test_independent_wire(gate.out);
    }
}

#include <gtest/gtest.h>

#include <iostream>
#include <thread>

#include <ATLab/benchmark.hpp>

#include <ATLab/preprocess.hpp>


const std::string ADDRESS {"127.0.0.1"};
constexpr unsigned short PORT {12345};

TEST(Preprocess, DEFAULT) {
    const auto circuit {ATLab::Circuit("circuits/bristol_format/sha-1.txt")};
    std::unique_ptr<ATLab::Garbler::PreprocessedData> pGarblerPreData;
    std::unique_ptr<ATLab::Evaluator::PreprocessedData> pEvaluatorPreData;

    std::thread garblerThread{[&](){
        BENCHMARK_INIT;
        BENCHMARK_START;
        emp::NetIO io(emp::NetIO::SERVER, ADDRESS, PORT, true);
        pGarblerPreData = std::make_unique<ATLab::Garbler::PreprocessedData>(
            ATLab::Garbler::preprocess(io, circuit)
        );
        BENCHMARK_END(Garbler)
    }}, evaluatorThread{[&]() {
        BENCHMARK_INIT;
        BENCHMARK_START;
        emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, PORT, true);
        pEvaluatorPreData = std::make_unique<ATLab::Evaluator::PreprocessedData>(
            ATLab::Evaluator::preprocess(io, circuit)
        );
        BENCHMARK_END(Evaluator)
    }};

    garblerThread.join();
    evaluatorThread.join();

    EXPECT_EQ(pGarblerPreData->matrix.rowSize, pEvaluatorPreData->matrix.rowSize);
    EXPECT_EQ(pGarblerPreData->matrix.colSize, pEvaluatorPreData->matrix.colSize);
    EXPECT_EQ(pGarblerPreData->matrix.data, pEvaluatorPreData->matrix.data);
    const auto& authedBits {pEvaluatorPreData->b};
    const auto& keys {pGarblerPreData->bKeys};

    ASSERT_EQ(authedBits.global_key_size(), 1);
    ASSERT_EQ(authedBits.size(), keys.size());

    const auto& globalKey {keys.get_global_key(0)};

    for (size_t i = 0; i < authedBits.size(); ++i) {
        emp::block expected = keys.get_local_key(0, i);
        if (authedBits.at(i)) {
            expected = expected ^ globalKey;
        }
        ASSERT_EQ(ATLab::as_uint128(expected), ATLab::as_uint128(authedBits.get_mac(0, i)));
    }
}

#include <array>
#include <chrono>
#include <vector>

#include <gtest/gtest.h>
#include <emp-tool/utils/block.h>
#include <emp-tool/utils/f2k.h>

#include "../include/ATLab/PRNG.hpp"
#include "../include/ATLab/utils.hpp"

namespace {
constexpr size_t kVectorSize {40};
constexpr size_t kIterations {4096};
constexpr size_t kRepetitions {256};
constexpr size_t kTrials {5};

alignas(16) volatile emp::block g_sink {emp::zero_block};

void consume(emp::block value) {
    auto current = g_sink;
    current = _mm_xor_si128(current, value);
    g_sink = current;
}

std::vector<std::array<emp::block, kVectorSize>> generate_dataset() {
    std::vector<std::array<emp::block, kVectorSize>> dataset(kIterations);
    auto& prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};
    for (auto& row : dataset) {
        for (auto& element : row) {
            element = ATLab::as_block(prng());
        }
    }
    return dataset;
}

using Clock = std::chrono::steady_clock;
using Duration = std::chrono::nanoseconds;

template <typename Runner>
Duration measure_runner(Runner&& runner) {
    Duration best {Duration::max()};
    for (size_t trial {0}; trial < kTrials; ++trial) {
        const auto start {Clock::now()};
        for (size_t repetition {0}; repetition < kRepetitions; ++repetition) {
            consume(runner());
        }
        const auto elapsed {std::chrono::duration_cast<Duration>(Clock::now() - start)};
        if (elapsed < best) {
            best = elapsed;
        }
    }
    return best;
}

} // namespace

TEST(VectorInnerProduction, BenchmarkAgainstEmp) {
    auto lhs {generate_dataset()};
    auto rhs {generate_dataset()};

    auto run_emp = [&]() {
        emp::block accumulator {emp::zero_block};
        for (size_t i {0}; i < kIterations; ++i) {
            emp::block product;
            emp::vector_inn_prdt_sum_red(&product, lhs[i].data(), rhs[i].data(), kVectorSize);
            accumulator = accumulator ^ product;
        }
        return accumulator;
    };

    auto run_atlab = [&]() {
        emp::block accumulator {emp::zero_block};
        for (size_t i {0}; i < kIterations; ++i) {
            const auto product {ATLab::vector_inner_product<kVectorSize>(lhs[i].data(), rhs[i].data())};
            accumulator = accumulator ^ product;
        }
        return accumulator;
    };

    EXPECT_EQ(ATLab::as_uint128(run_emp()), ATLab::as_uint128(run_atlab()));

    const auto baseline_duration {measure_runner(run_emp)};
    const auto optimized_duration {measure_runner(run_atlab)};

    std::clog << "emp::vector_inn_prdt_sum_red cumulative time: "
              << baseline_duration.count() << " ns\n";
    std::clog << "ATLab::vector_inner_product cumulative time: "
              << optimized_duration.count() << " ns\n";

    EXPECT_LT(optimized_duration, baseline_duration);
}

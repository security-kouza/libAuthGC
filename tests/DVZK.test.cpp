#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <thread>
#include <vector>

#include <emp-tool/utils/block.h>

#include <DVZK.hpp>
#include <PRNG.hpp>
#include <authed_bit.hpp>
#include <block_correlated_OT.hpp>
#include <utils.hpp>

namespace {
    const std::string ADDRESS {"127.0.0.1"};
    constexpr unsigned short DVZK_PORT {12360};
    constexpr size_t GLOBAL_KEY_SIZE {1};
    constexpr size_t BLOCK_SIZE {40};
}

TEST(DVZK, default) {
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};

    std::thread verifierThread {
        [&prng]() {
            std::vector<emp::block> deltaArr(GLOBAL_KEY_SIZE);
            for (auto& delta : deltaArr) {
                delta = ATLab::as_block(prng());
            }

            emp::NetIO io(emp::NetIO::SERVER, ADDRESS, DVZK_PORT, true);
            ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);

            const ATLab::ITMacBlockKeys aKeys {io, sender, BLOCK_SIZE},
                bKeys {io, sender, BLOCK_SIZE},
                cKeys {io, sender, BLOCK_SIZE},
                randomKeys {io, sender, BLOCK_SIZE};

            EXPECT_NO_THROW(
                ATLab::DVZK::verify<BLOCK_SIZE>(io, sender, aKeys, bKeys, cKeys)
            );
            EXPECT_THROW(
                ATLab::DVZK::verify<BLOCK_SIZE>(io, sender, aKeys, bKeys, randomKeys),
                std::runtime_error
            );
        }
    };

    std::thread proverThread {
        [&prng]() {
            std::vector<emp::block>
                aBlocks(BLOCK_SIZE),
                bBlocks(BLOCK_SIZE),
                cBlocks(BLOCK_SIZE),
                randomBlocks(BLOCK_SIZE);

            for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                aBlocks.at(i) = ATLab::as_block(prng());
                bBlocks.at(i) = ATLab::as_block(prng());
                randomBlocks.at(i) = ATLab::as_block(prng());
                emp::gfmul(aBlocks.at(i), bBlocks.at(i), &cBlocks.at(i));
            }
            emp::NetIO io(emp::NetIO::CLIENT, ADDRESS, DVZK_PORT, true);
            ATLab::BlockCorrelatedOT::Receiver receiver(io, GLOBAL_KEY_SIZE);

            const ATLab::ITMacBlocks aAuth {io, receiver, aBlocks},
                bAuth {io, receiver, bBlocks},
                cAuth {io, receiver, cBlocks},
                randomAuth {io, receiver, randomBlocks};

            ATLab::DVZK::prove<BLOCK_SIZE>(io, receiver, aAuth, bAuth, cAuth);
            ATLab::DVZK::prove<BLOCK_SIZE>(io, receiver, aAuth, bAuth, randomAuth);
        }
    };

    verifierThread.join();
    proverThread.join();
}

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <optional>
#include <thread>
#include <vector>

#include <emp-tool/utils/block.h>

#include <../include/ATLab/DVZK.hpp>
#include <../include/ATLab/PRNG.hpp>
#include <ATLab/authed_bit.hpp>
#include <../include/ATLab/block_correlated_OT.hpp>
#include <../include/ATLab/utils.hpp>
#include "test-helper.hpp"

namespace {
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

            auto& io {server_io()};
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
            io.flush();
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
            auto& io {client_io()};
            ATLab::BlockCorrelatedOT::Receiver receiver(io, GLOBAL_KEY_SIZE);

            const ATLab::ITMacBlocks aAuth {io, receiver, aBlocks},
                bAuth {io, receiver, bBlocks},
                cAuth {io, receiver, cBlocks},
                randomAuth {io, receiver, randomBlocks};

            ATLab::DVZK::prove<BLOCK_SIZE>(io, receiver, aAuth, bAuth, cAuth);
            ATLab::DVZK::prove<BLOCK_SIZE>(io, receiver, aAuth, bAuth, randomAuth);
            io.flush();
        }
    };

    verifierThread.join();
    proverThread.join();
}

TEST(DVZK, bits_and_constant) {
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};

    std::thread verifierThread {
        [prng]() mutable {
            std::vector<emp::block> deltaArr(GLOBAL_KEY_SIZE);
            for (auto& delta : deltaArr) {
                delta = ATLab::as_block(prng());
            }

            auto& io {server_io()};
            ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);

            const ATLab::ITMacBitKeys aKeys {io, sender, BLOCK_SIZE};

            const ATLab::ITMacBlockKeys
                bKeys {io, sender, 1},
                cKeys {io, sender, BLOCK_SIZE},
                randomKeys {io, sender, BLOCK_SIZE};

            EXPECT_NO_THROW(
                ATLab::DVZK::verify<BLOCK_SIZE>(io, sender, aKeys, bKeys, cKeys)
            );
            EXPECT_THROW(
                ATLab::DVZK::verify<BLOCK_SIZE>(io, sender, aKeys, bKeys, randomKeys),
                std::runtime_error
            );
            io.flush();
        }
    };

    std::thread proverThread {
        [prng]() mutable {
            const auto aBits {ATLab::random_dynamic_bitset(BLOCK_SIZE)};
            const auto bBlock {ATLab::as_block(prng())};

            std::vector<emp::block>
                cBlocks(BLOCK_SIZE, _mm_set_epi64x(0, 0)),
                randomBlocks(BLOCK_SIZE);

            for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                randomBlocks.at(i) = ATLab::as_block(prng());
                if (aBits[i]) {
                    cBlocks[i] = bBlock;
                }
            }
            auto& io {client_io()};
            ATLab::BlockCorrelatedOT::Receiver receiver(io, GLOBAL_KEY_SIZE);

            const auto aAuth {ATLab::ITMacBits{io, receiver, aBits}};
            const ATLab::ITMacBlocks
                bAuth {io, receiver, {bBlock}},
                cAuth {io, receiver, cBlocks},
                randomAuth {io, receiver, randomBlocks};

            ATLab::DVZK::prove<BLOCK_SIZE>(io, receiver, aAuth, bAuth, cAuth);
            ATLab::DVZK::prove<BLOCK_SIZE>(io, receiver, aAuth, bAuth, randomAuth);
            io.flush();
        }
    };

    verifierThread.join();
    proverThread.join();
}

TEST(DVZK, dynamic_bits_and_constant) {
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};

    std::thread verifierThread {
        [prng]() mutable {
            std::vector<emp::block> deltaArr(GLOBAL_KEY_SIZE);
            for (auto& delta : deltaArr) {
                delta = ATLab::as_block(prng());
            }

            auto& io {server_io()};
            ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);

            const ATLab::ITMacBitKeys aKeys {io, sender, BLOCK_SIZE};

            const ATLab::ITMacBlockKeys
                bKeys {io, sender, 1},
                cKeys {io, sender, BLOCK_SIZE},
                randomKeys {io, sender, BLOCK_SIZE};

            EXPECT_NO_THROW(
                ATLab::DVZK::verify(io, sender, aKeys, bKeys, cKeys, BLOCK_SIZE)
            );
            EXPECT_THROW(
                ATLab::DVZK::verify(io, sender, aKeys, bKeys, randomKeys, BLOCK_SIZE),
                std::runtime_error
            );
            io.flush();
        }
    };

    std::thread proverThread {
        [prng]() mutable {
            const auto aBits {ATLab::random_dynamic_bitset(BLOCK_SIZE)};
            const auto bBlock {ATLab::as_block(prng())};

            std::vector<emp::block>
                cBlocks(BLOCK_SIZE, _mm_set_epi64x(0, 0)),
                randomBlocks(BLOCK_SIZE);

            for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                randomBlocks.at(i) = ATLab::as_block(prng());
                if (aBits[i]) {
                    cBlocks[i] = bBlock;
                }
            }
            auto& io {client_io()};
            ATLab::BlockCorrelatedOT::Receiver receiver(io, GLOBAL_KEY_SIZE);

            const auto aAuth {ATLab::ITMacBits{io, receiver, aBits}};
            const ATLab::ITMacBlocks
                bAuth {io, receiver, {bBlock}},
                cAuth {io, receiver, cBlocks},
                randomAuth {io, receiver, randomBlocks};

            ATLab::DVZK::prove(io, receiver, aAuth, bAuth, cAuth, BLOCK_SIZE);
            ATLab::DVZK::prove(io, receiver, aAuth, bAuth, randomAuth, BLOCK_SIZE);
            io.flush();
        }
    };

    verifierThread.join();
    proverThread.join();
}

TEST(DVZK, streaming_inner_product) {
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};

    std::thread verifierThread {
        [&prng]() {
            std::vector<emp::block> deltaArr(GLOBAL_KEY_SIZE);
            for (auto& delta : deltaArr) {
                delta = ATLab::as_block(prng());
            }

            auto& io {server_io()};
            ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);

            const ATLab::ITMacBlockKeys
                aKeys {io, sender, BLOCK_SIZE},
                bKeys {io, sender, BLOCK_SIZE},
                cKeys {io, sender, BLOCK_SIZE};

            const auto delta {deltaArr.at(0)};

            {
                ATLab::DVZK::Verifier streamingVerifier {io, sender};
                for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                    streamingVerifier.update({
                        aKeys.get_local_key(0, i),
                        bKeys.get_local_key(0, i),
                        cKeys.get_local_key(0, i)
                    });
                }
                EXPECT_NO_THROW(streamingVerifier.verify(io));
            }

            {
                ATLab::DVZK::Verifier tamperedVerifier {io, sender};
                for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                    tamperedVerifier.update({
                        aKeys.get_local_key(0, i),
                        bKeys.get_local_key(0, i),
                        cKeys.get_local_key(0, i)
                    });
                }
                EXPECT_THROW(tamperedVerifier.verify(io), std::runtime_error);
            }
            io.flush();
        }
    };

    std::thread proverThread {
        [&prng]() {
            std::vector<emp::block>
                aBlocks(BLOCK_SIZE),
                bBlocks(BLOCK_SIZE),
                cBlocks(BLOCK_SIZE),
                tamperedMacs(BLOCK_SIZE);

            for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                aBlocks.at(i) = ATLab::as_block(prng());
                bBlocks.at(i) = ATLab::as_block(prng());
                tamperedMacs.at(i) = ATLab::as_block(prng());
                emp::gfmul(aBlocks.at(i), bBlocks.at(i), &cBlocks.at(i));
            }
            auto& io {client_io()};
            ATLab::BlockCorrelatedOT::Receiver receiver(io, GLOBAL_KEY_SIZE);

            const ATLab::ITMacBlocks
                aAuth {io, receiver, aBlocks},
                bAuth {io, receiver, bBlocks},
                cAuth {io, receiver, cBlocks};

            {
                ATLab::DVZK::Prover streamingProver {io, receiver};
                for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                    streamingProver.update(
                        {
                            aAuth.get_block(i),
                            bAuth.get_block(i),
                            cAuth.get_block(i)
                        },
                        {
                            aAuth.get_mac(0, i),
                            bAuth.get_mac(0, i),
                            cAuth.get_mac(0, i)
                        }
                    );
                }
                streamingProver.prove(io);
            }

            {
                ATLab::DVZK::Prover tamperedProver {io, receiver};
                for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                    tamperedProver.update(
                        {
                            aAuth.get_block(i),
                            bAuth.get_block(i),
                            cAuth.get_block(i)
                        },
                        {
                            aAuth.get_mac(0, i),
                            bAuth.get_mac(0, i),
                            tamperedMacs.at(i)
                        }
                    );
                }
                tamperedProver.prove(io);
            }
            io.flush();
        }
    };

    verifierThread.join();
    proverThread.join();
}

TEST(DVZK, streaming_bits) {
    auto prng {ATLab::PRNG_Kyber::get_PRNG_Kyber()};

    std::thread verifierThread {
        [&prng]() {
            std::vector<emp::block> deltaArr(GLOBAL_KEY_SIZE);
            for (auto& delta : deltaArr) {
                delta = ATLab::as_block(prng());
            }

            auto& io {server_io()};
            ATLab::BlockCorrelatedOT::Sender sender(io, deltaArr);

            const ATLab::ITMacBitKeys
                aKeys {io, sender, BLOCK_SIZE},
                bKeys {io, sender, BLOCK_SIZE},
                cKeys {io, sender, BLOCK_SIZE};

            const auto delta {deltaArr.at(0)};

            {
                ATLab::DVZK::Verifier streamingVerifier {io, sender};
                for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                    streamingVerifier.update({
                        aKeys.get_local_key(0, i),
                        bKeys.get_local_key(0, i),
                        cKeys.get_local_key(0, i)
                    });
                }
                EXPECT_NO_THROW(streamingVerifier.verify(io));
            }

            {
                ATLab::DVZK::Verifier tamperedVerifier {io, sender};
                for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                    tamperedVerifier.update({
                        aKeys.get_local_key(0, i),
                        bKeys.get_local_key(0, i),
                        cKeys.get_local_key(0, i)
                    });
                }
                EXPECT_THROW(tamperedVerifier.verify(io), std::runtime_error);
            }
            io.flush();
        }
    };

    std::thread proverThread {
        [&prng]() {
            const auto aBits {ATLab::random_dynamic_bitset(BLOCK_SIZE)};
            const auto bBits {ATLab::random_dynamic_bitset(BLOCK_SIZE)};
            ATLab::Bitset cBits {aBits};
            cBits &= bBits;

            std::vector<emp::block> tamperedMacs(BLOCK_SIZE);
            for (auto& mac : tamperedMacs) {
                mac = ATLab::as_block(prng());
            }

            auto& io {client_io()};
            ATLab::BlockCorrelatedOT::Receiver receiver(io, GLOBAL_KEY_SIZE);

            ATLab::ITMacBits aAuth {io, receiver, aBits};
            ATLab::ITMacBits bAuth {io, receiver, bBits};
            ATLab::ITMacBits cAuth {io, receiver, cBits};

            {
                ATLab::DVZK::Prover streamingProver {io, receiver};
                for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                    const std::array<bool, 3> authedBits {
                        aBits[i],
                        bBits[i],
                        cBits[i]
                    };
                    const std::array<emp::block, 3> macs {
                        aAuth.get_mac(0, i),
                        bAuth.get_mac(0, i),
                        cAuth.get_mac(0, i)
                    };
                    streamingProver.update(authedBits, macs);
                }
                streamingProver.prove(io);
            }

            {
                ATLab::DVZK::Prover tamperedProver {io, receiver};
                for (size_t i {0}; i != BLOCK_SIZE; ++i) {
                    const std::array<bool, 3> authedBits {
                        aBits[i],
                        bBits[i],
                        cBits[i]
                    };
                    const std::array<emp::block, 3> macs {
                        aAuth.get_mac(0, i),
                        bAuth.get_mac(0, i),
                        tamperedMacs.at(i)
                    };
                    tamperedProver.update(authedBits, macs);
                }
                tamperedProver.prove(io);
            }
            io.flush();
        }
    };

    verifierThread.join();
    proverThread.join();
}

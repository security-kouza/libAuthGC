#ifndef ATLab_GLOBAL_KEY_SAMPLING_HPP
#define ATLab_GLOBAL_KEY_SAMPLING_HPP

#include <bitset>

#include <immintrin.h>

#include "authed_bit.hpp"
#include "PRNG.hpp"
#include "block_correlated_OT.hpp"
#include "params.hpp"

namespace ATLab::GlobalKeySampling {
    // PA
    class Garbler {
        emp::block _delta, _alpha_0;
        std::unique_ptr<BlockCorrelatedOT::Sender> _pSid0;

        BlockCorrelatedOT::Sender& sid0() const {
#ifdef DEBUG
            assert(_pSid0 != nullptr);
#endif // DEBUG
            return *_pSid0;
        }
    public:
        explicit Garbler(emp::NetIO& io) :
            _alpha_0 {}
        {
            __uint128_t delta {PRNG_Kyber::get_PRNG_Kyber()() | 1};
            _delta = as_block(delta);
            _pSid0 = std::make_unique<BlockCorrelatedOT::Sender>(io, std::vector{_delta});

            // 1
            const auto uLocalKeys {_pSid0->extend(STATISTICAL_SECURITY)};

            // 2
            std::bitset<STATISTICAL_SECURITY> lsbsOfULocalKeys;
            for (size_t i = 0; i < STATISTICAL_SECURITY; ++i) {
                lsbsOfULocalKeys[i] = get_LSB(uLocalKeys.at(i));
            }
            io.send_data(&lsbsOfULocalKeys, sizeof(lsbsOfULocalKeys));

            // 3
            auto authedDeltaB {ITMacBlockKeys{io, *_pSid0}};
            const uint8_t lsbKDeltaB {get_LSB(authedDeltaB.get_local_key(0))};
            uint8_t lsbMDeltaB;
            io.send_data(&lsbKDeltaB, sizeof(lsbKDeltaB));
            io.recv_data(&lsbMDeltaB, sizeof(lsbMDeltaB));
            if (lsbKDeltaB == lsbMDeltaB) {
                authedDeltaB.flip_block_lsb();
            }

            // Skipping 4, 5

            // 6a
        }
    };

    class Evaluator {
        emp::block _delta, _beta_0;
        BlockCorrelatedOT::Receiver _sid0;
    public:
        explicit Evaluator(emp::NetIO& io) :
            _delta {as_block(PRNG_Kyber::get_PRNG_Kyber()())},
            _beta_0 {},
            _sid0 {io, 1}
        {
            // 1
            const auto [uArr, uMacArr] {_sid0.extend(STATISTICAL_SECURITY)};

            // 2
            std::bitset<STATISTICAL_SECURITY> expectedLsb, receivedLsbs;
            for (size_t i = 0; i < STATISTICAL_SECURITY; ++i) {
                expectedLsb[i] = get_LSB(uMacArr.at(i)) ^ uArr.at(i);
            }
            io.recv_data(&receivedLsbs, sizeof(receivedLsbs));
            if (receivedLsbs != expectedLsb) {
                throw std::runtime_error{"Malicious check failed: LSB of ΔA might not be 1."};
            }

            // 3
            auto authedDeltaB {ITMacBlocks{io, _sid0, _delta}};
            const uint8_t lsbMDeltaB {get_LSB(authedDeltaB.get_mac(0))};
            uint8_t lsbKDeltaB;
            // TODO: parallel send
            io.recv_data(&lsbKDeltaB, sizeof(lsbKDeltaB));
            io.send_data(&lsbMDeltaB, sizeof(lsbMDeltaB));
            if (lsbKDeltaB == lsbMDeltaB) {
                authedDeltaB.flip_block_lsb();
                _delta = authedDeltaB.get_block();
            }

            // Skipping 4, 5
        }
    };
}

#endif // ATLab_GLOBAL_KEY_SAMPLING_HPP

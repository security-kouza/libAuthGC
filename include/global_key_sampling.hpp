#ifndef ATLab_GLOBAL_KEY_SAMPLING_HPP
#define ATLab_GLOBAL_KEY_SAMPLING_HPP

#include <bitset>

#include <immintrin.h>

#include "PRNG.hpp"
#include "block_correlated_OT.hpp"
#include "params.hpp"

namespace ATLab::GlobalKeySampling {
    using int128 = __uint128_t;

    class Garbler {
        int128 _delta, _alpha_0;
        std::unique_ptr<BlockCorrelatedOT::Sender> _pSid0;

        BlockCorrelatedOT::Sender& sid0() const {
#ifdef DEBUG
            assert(_pSid0 != nullptr);
#endif // DEBUG
            return *_pSid0;
        }
    public:
        explicit Garbler(emp::NetIO& io) :
            _delta {PRNG_Kyber::get_PRNG_Kyber()()},
            _alpha_0 {}
        {
            _delta |= 1;
            _pSid0 = std::make_unique<BlockCorrelatedOT::Sender>(io, std::vector{as_block(_delta)});

            // 1
            const auto uLocalKeys {_pSid0->extend(STATISTICAL_SECURITY)};

            // 2
            std::bitset<STATISTICAL_SECURITY> lsbsOfULocalKeys;
            for (size_t i = 0; i < STATISTICAL_SECURITY; ++i) {
                lsbsOfULocalKeys[i] = _mm_extract_epi8(uLocalKeys.at(i), 0) & 1;
            }
            io.send_data(&lsbsOfULocalKeys, sizeof(lsbsOfULocalKeys));
        }
    };

    class Evaluator {
        int128 _delta, _beta_0;
        BlockCorrelatedOT::Receiver _sid0;
    public:
        explicit Evaluator(emp::NetIO& io) :
            _delta {PRNG_Kyber::get_PRNG_Kyber()()},
            _beta_0 {},
            _sid0 {io, 1}
        {
            // 1
            const auto [uArr, uMacArr] {_sid0.extend(STATISTICAL_SECURITY)};

            // 2
            std::bitset<STATISTICAL_SECURITY> expectedLsb, receivedLsbs;
            for (size_t i = 0; i < STATISTICAL_SECURITY; ++i) {
                expectedLsb[i] = (_mm_extract_epi8(uMacArr.at(i), 0) & 1) ^ uArr.at(i);
            }
            io.recv_data(&receivedLsbs, sizeof(receivedLsbs));
            if (receivedLsbs != expectedLsb) {
                throw std::runtime_error{"Malicious check failed: LSB of ΔA might not be 1."};
            }
        }
    };
}

#endif // ATLab_GLOBAL_KEY_SAMPLING_HPP

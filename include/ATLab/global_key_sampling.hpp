#ifndef ATLab_GLOBAL_KEY_SAMPLING_HPP
#define ATLab_GLOBAL_KEY_SAMPLING_HPP

#include <bitset>

#include <immintrin.h>

#include <emp-tool/utils/hash.h>

#include "authed_bit.hpp"
#include "PRNG.hpp"
#include "block_correlated_OT.hpp"
#include "DVZK.hpp"
#include "params.hpp"
#include "util_protocols.hpp"

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
        explicit Garbler(ATLab::NetIO& io) {
            const auto high {THE_GLOBAL_PRNG()}, low {THE_GLOBAL_PRNG() | 1};
            _delta = _mm_set_epi64x(static_cast<long long>(high), static_cast<long long>(low));
            _pSid0 = std::make_unique<BlockCorrelatedOT::Sender>(io, std::vector{_delta});

            // 1
            const auto uLocalKeys {_pSid0->extend(STATISTICAL_SECURITY)};

            // 2
            std::bitset<STATISTICAL_SECURITY> lsbsOfULocalKeys;
            for (size_t i = 0; i < STATISTICAL_SECURITY; ++i) {
                lsbsOfULocalKeys[i] = get_LSB(uLocalKeys[i]);
            }
            io.send_data(&lsbsOfULocalKeys, sizeof(lsbsOfULocalKeys));

            // 3
            auto deltaBKey {ITMacBlockKeys{io, *_pSid0, 1}};
            const uint8_t lsbKDeltaB {get_LSB(deltaBKey.get_local_key(0))};
            uint8_t lsbMDeltaB;
            io.send_data(&lsbKDeltaB, sizeof(lsbKDeltaB));
            io.recv_data(&lsbMDeltaB, sizeof(lsbMDeltaB));
            if (lsbKDeltaB == lsbMDeltaB) {
                deltaBKey.flip_block_lsb();
            }

            // extra step
            _alpha_0 = deltaBKey.get_local_key(0, 0);

            // Skipping 4, 5
            BlockCorrelatedOT::Receiver cotDeltaB {io, 1}; // sid0'

            // 6a
            const auto xKeys {ITMacBitKeys{*_pSid0, STATISTICAL_SECURITY}};
            const auto xDeltaBKeys {ITMacBlockKeys{io, *_pSid0, STATISTICAL_SECURITY}};
            DVZK::verify<STATISTICAL_SECURITY>(io, *_pSid0, xKeys, deltaBKey, xDeltaBKeys);

            // b
            std::bitset<STATISTICAL_SECURITY> lsbsOfXDeltaBKeys;
            for (size_t i {0}; i != STATISTICAL_SECURITY; ++i) {
                lsbsOfXDeltaBKeys[i] = get_LSB(xDeltaBKeys.get_local_key(0, i));
            }
            io.send_data(&lsbsOfXDeltaBKeys, sizeof(lsbsOfXDeltaBKeys));

            // c
            ITMacBlocks authedDeltaA {io, cotDeltaB, {_delta}};

            // d
            const ITMacBits authedY {cotDeltaB, STATISTICAL_SECURITY};

            std::vector<emp::block> yDeltaA;
            yDeltaA.reserve(STATISTICAL_SECURITY);
            for (size_t i {0}; i != STATISTICAL_SECURITY; ++i) {
                yDeltaA.push_back(authedY[i] ? _delta : _mm_set_epi64x(0, 0));
            }
            const ITMacBlocks authedYDeltaA {io, cotDeltaB, std::move(yDeltaA)};

            DVZK::prove<STATISTICAL_SECURITY>(io, cotDeltaB, authedY, authedDeltaA, authedYDeltaA);

            // e
            std::bitset<STATISTICAL_SECURITY> lsbsOfYDeltaAKeys, expectedLsbsOfYDeltaAKeys;
            for (size_t i {0}; i != STATISTICAL_SECURITY; ++i) {
                expectedLsbsOfYDeltaAKeys[i] = get_LSB(authedYDeltaA.get_mac(0, i)) ^ authedY[i];
            }
            io.recv_data(&lsbsOfYDeltaAKeys, sizeof(lsbsOfYDeltaAKeys));
            if (lsbsOfYDeltaAKeys != expectedLsbsOfYDeltaAKeys) {
                throw std::runtime_error{"Garbler: lsb(ΔA * ΔB) is not 1."};
            }

            // f, g
            const emp::block toCompare {_mm_xor_si128(deltaBKey.get_local_key(0, 0), authedDeltaA.get_mac(0, 0))};
            if (compare_hash_low(io, &toCompare, sizeof(toCompare))) {
                throw std::runtime_error{"ΔB not consistent"};
            }
        }

        const emp::block& get_delta() const {
            return _delta;
        }

        const emp::block& get_alpha_0() const {
            return _alpha_0;
        }

        BlockCorrelatedOT::Sender& get_COT_sender() const {
            return *_pSid0;
        }
    };

    class Evaluator {
        emp::block _delta, _beta_0;
        BlockCorrelatedOT::Receiver _sid0;
    public:
        explicit Evaluator(ATLab::NetIO& io) :
            _delta {_mm_set_epi64x(
                static_cast<long long>(THE_GLOBAL_PRNG()),
                static_cast<long long>(THE_GLOBAL_PRNG())
            )},
            _sid0 {io, 1}
        {
            // 1
            const auto [uArr, uMacArr] {_sid0.extend(STATISTICAL_SECURITY)};

            // 2
            std::bitset<STATISTICAL_SECURITY> expectedLsb, receivedLsbs;
            for (size_t i = 0; i < STATISTICAL_SECURITY; ++i) {
                expectedLsb[i] = get_LSB(uMacArr[i]) ^ uArr[i];
            }
            io.recv_data(&receivedLsbs, sizeof(receivedLsbs));
            if (receivedLsbs != expectedLsb) {
                throw std::runtime_error{"Malicious check failed: LSB of ΔA is not 1."};
            }

            // 3
            auto authedDeltaB {ITMacBlocks{io, _sid0, {_delta}}};
            const uint8_t lsbMDeltaB {get_LSB(authedDeltaB.get_mac(0))};
            uint8_t lsbKDeltaB;
            io.send_data(&lsbMDeltaB, sizeof(lsbMDeltaB));
            io.recv_data(&lsbKDeltaB, sizeof(lsbKDeltaB));
            if (lsbKDeltaB == lsbMDeltaB) {
                authedDeltaB.flip_block_lsb();
                _delta = authedDeltaB.get_block();
            }

            // extra step
            _beta_0 = authedDeltaB.get_mac(0, 0);

            // Skipping 4, 5
            BlockCorrelatedOT::Sender cotDeltaB {io, {_delta}}; // sid0'

            // 6a
            const auto authedX {ITMacBits{_sid0, STATISTICAL_SECURITY}};

            std::vector<emp::block> xDeltaB;
            xDeltaB.reserve(STATISTICAL_SECURITY);
            for (size_t i {0}; i != STATISTICAL_SECURITY; ++i) {
                xDeltaB.push_back(authedX[i] ? _delta : _mm_set_epi64x(0, 0));
            }
            const auto authedXDeltaB {ITMacBlocks{io, _sid0, std::move(xDeltaB)}};

            DVZK::prove<STATISTICAL_SECURITY>(io, _sid0, authedX, authedDeltaB, authedXDeltaB);

            // b
            std::bitset<STATISTICAL_SECURITY> lsbsofXDeltaBKeys, expectedLsbsOfXDeltaBKeys;
            for (size_t i {0}; i != STATISTICAL_SECURITY; ++i) {
                expectedLsbsOfXDeltaBKeys[i] = get_LSB(authedXDeltaB.get_mac(0, i)) ^ authedX[i];
            }
            io.recv_data(&lsbsofXDeltaBKeys, sizeof(lsbsofXDeltaBKeys));
            if (lsbsofXDeltaBKeys != expectedLsbsOfXDeltaBKeys) {
                throw std::runtime_error{"Evaluator: lsb(ΔA * ΔB) is not 1."};
            }

            //c
            ITMacBlockKeys deltaAKey {io, cotDeltaB, 1};

            // d
            const ITMacBitKeys yKeys {cotDeltaB, STATISTICAL_SECURITY};
            const ITMacBlockKeys yDeltaAKeys {io, cotDeltaB, STATISTICAL_SECURITY};
            DVZK::verify<STATISTICAL_SECURITY>(io, cotDeltaB, yKeys, deltaAKey, yDeltaAKeys);

            // e
            std::bitset<STATISTICAL_SECURITY> lsbsOfYDeltaAKeys;
            for (size_t i {0}; i != STATISTICAL_SECURITY; ++i) {
                lsbsOfYDeltaAKeys[i] = get_LSB(yDeltaAKeys.get_local_key(0, i));
            }
            io.send_data(&lsbsOfYDeltaAKeys, sizeof(lsbsOfYDeltaAKeys));

            // f, g
            const emp::block toCompare {_mm_xor_si128(deltaAKey.get_local_key(0, 0), authedDeltaB.get_mac(0, 0))};
            // std::array<uint8_t, DIGEST_SIZE> hashRes {};
            // emp::Hash::hash_once(hashRes.data(), &toCompare, sizeof(toCompare));
            // std::array<uint8_t, HALF_DIGEST_SIZE> highHash {};
            // io.send_data(hashRes.data() + HALF_DIGEST_SIZE, HALF_DIGEST_SIZE);
            // io.recv_data(highHash.data(), HALF_DIGEST_SIZE);
            //
            // const auto expectedHigh {*reinterpret_cast<const std::array<uint8_t, HALF_DIGEST_SIZE>*>(hashRes.data())};
            // if (expectedHigh != highHash) {
            if (compare_hash_low(io, &toCompare, sizeof(toCompare))) {
                throw std::runtime_error{"ΔA not consistent"};
            }
        }

        const emp::block& get_delta() const {
            return _delta;
        }

        const emp::block& get_beta_0() const {
            return _beta_0;
        }

        BlockCorrelatedOT::Receiver& get_COT_receiver() {
            return _sid0;
        }
    };
}

#endif // ATLab_GLOBAL_KEY_SAMPLING_HPP

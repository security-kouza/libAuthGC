#ifndef ATLab_DVZK_HPP
#define ATLab_DVZK_HPP

#include <array>
#include <vector>

#include "net-io.hpp"
#include <emp-tool/utils/f2k.h>

#include "utils.hpp"

#include "block_correlated_OT.hpp"
#include "authed_bit.hpp"

namespace ATLab::DVZK {
    // proving x[i] y[i] = z[i] with dynamic size
    class Prover {
        const ITMacBlocks _authedBlock;
        emp::block A0_, A1_;
        std::unique_ptr<emp::PRG> _pChalGen;
    public:
        Prover(ATLab::NetIO& io, const BlockCorrelatedOT::Receiver& bCOTReceiver):
            _authedBlock {ITMacBlocks{bCOTReceiver, 1}},
            A0_ {_authedBlock.get_mac(0, 0)},
            A1_ {_authedBlock.get_block(0)}
        {
            emp::block challengeSeed;
            io.recv_data(&challengeSeed, sizeof(challengeSeed));
            _pChalGen = std::make_unique<emp::PRG>(&challengeSeed);
        }

        // authedBlocks[0] * authedBlocks[1] == authedBlocks[2]
        void update(const std::array<emp::block, 3>& authedBlocks, const std::array<emp::block, 3>& macs) noexcept {
            assert(as_uint128(authedBlocks[2]) == as_uint128(
                gf_mul_block(authedBlocks[0], authedBlocks[1]))
            );

            emp::block challenge;
            _pChalGen->random_block(&challenge, 1);

            emp::block macProduct;
            emp::gfmul(macs[0], macs[1], &macProduct);
            emp::block a0Contribution;
            emp::gfmul(challenge, macProduct, &a0Contribution);
            xor_to(A0_, a0Contribution);

            emp::block prodXMacOfY, prodYMacOfX;
            emp::gfmul(authedBlocks[0], macs[1], &prodXMacOfY);
            emp::gfmul(authedBlocks[1], macs[0], &prodYMacOfX);
            emp::block tmp {_mm_xor_si128(macs[2], prodXMacOfY)};
            tmp = _mm_xor_si128(tmp, prodYMacOfX);

            emp::block a1Contribution;
            emp::gfmul(challenge, tmp, &a1Contribution);
            xor_to(A1_, a1Contribution);
        }

        // authedBits[0] * authedBits[1] == authedBits[2]
        void update(const std::array<bool, 3> authedBits, const std::array<emp::block, 3>& macs) noexcept {
            assert(authedBits[2] == (authedBits[0] && authedBits[1]));

            emp::block challenge;
            _pChalGen->random_block(&challenge, 1);

            emp::block macProduct;
            emp::gfmul(macs[0], macs[1], &macProduct);
            emp::block a0Contribution;
            emp::gfmul(challenge, macProduct, &a0Contribution);
            xor_to(A0_, a0Contribution);

            const emp::block prodXMacOfY {and_all_bits(authedBits[0], macs[1])};
            const emp::block prodYMacOfX {and_all_bits(authedBits[1], macs[0])};
            emp::block tmp {_mm_xor_si128(macs[2], prodXMacOfY)};
            tmp = _mm_xor_si128(tmp, prodYMacOfX);

            emp::block a1Contribution;
            emp::gfmul(challenge, tmp, &a1Contribution);
            xor_to(A1_, a1Contribution);
        }


        void prove(ATLab::NetIO& io) const noexcept {
            io.send_data(&A0_, sizeof(A0_));
            io.send_data(&A1_, sizeof(A1_));
        }
    };

    // verifying x[i] y[i] = z[i] with dynamic size
    class Verifier {
        const emp::block delta_;
        const ITMacBlockKeys _authedBlockKey;
        emp::block B_;
        std::unique_ptr<emp::PRG> _pChalGen;
    public:
        Verifier(ATLab::NetIO& io, const BlockCorrelatedOT::Sender& bCOTSender):
            delta_ {bCOTSender.get_delta(0)},
            _authedBlockKey {ITMacBlockKeys{bCOTSender, 1}},
            B_ {_authedBlockKey.get_local_key(0, 0)}
        {
            emp::block challengeSeed {_mm_set_epi64x(THE_GLOBAL_PRNG(), THE_GLOBAL_PRNG())};
            io.send_data(&challengeSeed, sizeof(challengeSeed));
            _pChalGen = std::make_unique<emp::PRG>(&challengeSeed);
        }

        void update(const std::array<emp::block, 3>& localKeys) noexcept {
            emp::block challenge;
            _pChalGen->random_block(&challenge, 1);

            emp::block prodTwoKeys, prodZWithDelta;
            emp::gfmul(localKeys[0], localKeys[1], &prodTwoKeys);
            emp::gfmul(localKeys[2], delta_, &prodZWithDelta);
            const emp::block diff {_mm_xor_si128(prodTwoKeys, prodZWithDelta)};

            emp::block contribution;
            emp::gfmul(challenge, diff, &contribution);
            xor_to(B_, contribution);
        }

        void verify(ATLab::NetIO& io) const {
            emp::block A0, A1;
            io.recv_data(&A0, sizeof(A0));
            io.recv_data(&A1, sizeof(A1));

            emp::block adjusted {B_};
            xor_to(adjusted, A0);

            emp::block expected;
            emp::gfmul(A1, delta_, &expected);
            if (emp::cmpBlock(&adjusted, &expected, 1) == false) {
                throw std::runtime_error{"Malicious behavior detected: DVZK verification failed."};
            }
        }
    };

    // proving x[i] y[i] = z[i]
    template<size_t blockSize>
    void prove(
        ATLab::NetIO& io,
        const BlockCorrelatedOT::Receiver& bCOTReceiver,
        const ITMacBlocks& x,
        const ITMacBlocks& y,
        const ITMacBlocks& z
    ) {
        const auto authedBlock {ITMacBlocks{bCOTReceiver, 1}};

        emp::block challengeSeed;
        io.recv_data(&challengeSeed, sizeof(challengeSeed));

        auto chalGen {emp::PRG{&challengeSeed}};
        std::array<emp::block, blockSize> challenges;
        chalGen.random_block(challenges.data(), blockSize);

        // A0
        std::array<emp::block, blockSize> macProd {};
        for (size_t i {0}; i != blockSize; ++i) {
            emp::gfmul(x.get_mac(0, i), y.get_mac(0, i), &macProd[i]);
        }
        emp::block A0 {vector_inner_product<blockSize>(challenges.data(), macProd.data())};
        A0 = _mm_xor_si128(A0, authedBlock.get_mac(0, 0));

        // A1
        std::array<emp::block, blockSize> tmp {};
        for (size_t i {0}; i != blockSize; ++i) {
            emp::block prodXMacOfY, prodYMacOfX;
            emp::gfmul(x.get_block(i), y.get_mac(0, i), &prodXMacOfY);
            emp::gfmul(y.get_block(i), x.get_mac(0, i), &prodYMacOfX);
            tmp[i] = _mm_xor_si128(z.get_mac(0, i), prodXMacOfY);
            tmp[i] = _mm_xor_si128(tmp[i], prodYMacOfX);
        }
        emp::block A1 {vector_inner_product<blockSize>(challenges.data(), tmp.data())};
        A1 = _mm_xor_si128(A1, authedBlock.get_block(0));

        io.send_data(&A0, sizeof(A0));
        io.send_data(&A1, sizeof(A1));
    }

    /**
     * @throw std::runtime_error if failed.
     */
    template <size_t blockSize>
    void verify(
        ATLab::NetIO& io,
        const BlockCorrelatedOT::Sender& bCOTSender,
        const ITMacBlockKeys& x,
        const ITMacBlockKeys& y,
        const ITMacBlockKeys& z
    ) {
        const emp::block delta  {x.get_global_key(0)};
#ifdef DEBUG
        if (as_uint128(delta) != as_uint128(y.get_global_key(0)) ||
            as_uint128(delta) != as_uint128(z.get_global_key(0))
        ) {
            throw std::invalid_argument{"x, y and z do not share the same global key."};
        }
#endif // DEBUG

        const auto authedBlockKey {ITMacBlockKeys{bCOTSender, 1}};

        const emp::block challengeSeed {_mm_set_epi64x(THE_GLOBAL_PRNG(), THE_GLOBAL_PRNG())};
        io.send_data(&challengeSeed, sizeof(challengeSeed));

        auto chalGen {emp::PRG{&challengeSeed}};

        std::array<emp::block, blockSize> challenges;
        chalGen.random_block(challenges.data(), blockSize);

        std::array<emp::block, blockSize> tmp {};
        for (size_t i {0}; i != blockSize; ++i) {
            emp::block prodTwoKeys, prodZWithDelta;
            emp::gfmul(x.get_local_key(0, i), y.get_local_key(0, i), &prodTwoKeys);
            emp::gfmul(z.get_local_key(0, i), delta, &prodZWithDelta);
            tmp[i] = _mm_xor_si128(prodTwoKeys, prodZWithDelta);
        }

        auto B {vector_inner_product<blockSize>(challenges.data(), tmp.data())};
        B = _mm_xor_si128(B, authedBlockKey.get_local_key(0, 0));

        emp::block A0, A1;
        io.recv_data(&A0, sizeof(A0));
        io.recv_data(&A1, sizeof(A1));

        B = _mm_xor_si128(B, A0);
        emp::block expected;
        emp::gfmul(A1, delta, &expected);
        if (emp::cmpBlock(&B, &expected, 1) == false) {
            throw std::runtime_error{"Malicious behavior detected: DVZK verification failed."};
        }
    }

    /**
     * Prove x[i] * y == z[i]
     * @tparam blockSize the size of z and x
     * @param x size must be blockSize, with only one global key
     * @param y size must be 1
     * @param z size must be blockSize, with only one global key
     */
    template<size_t blockSize>
    void prove(
        ATLab::NetIO& io,
        const BlockCorrelatedOT::Receiver& bCOTReceiver,
        const ITMacBits& x,
        const ITMacBlocks& y,
        const ITMacBlocks& z
    ) {
#ifdef DEBUG
        if (x.size() != z.size() || y.size() != 1) {
            throw std::invalid_argument{"Wrong parameter size."};
        }
#endif // DEBUG
        const emp::block& yValue {y.get_block(0)};
        const emp::block& yMac {y.get_mac(0, 0)};

        const auto authedBlock {ITMacBlocks{bCOTReceiver, 1}};

        emp::block challengeSeed;
        io.recv_data(&challengeSeed, sizeof(challengeSeed));

        auto chalGen {emp::PRG{&challengeSeed}};
        std::array<emp::block, blockSize> challenges;
        chalGen.random_block(challenges.data(), blockSize);

        // A0
        std::array<emp::block, blockSize> macProd {};
        for (size_t i {0}; i != blockSize; ++i) {
            emp::gfmul(x.get_mac(0, i), yMac, &macProd[i]);
        }
        emp::block A0 {vector_inner_product<blockSize>(challenges.data(), macProd.data())};
        A0 = _mm_xor_si128(A0, authedBlock.get_mac(0, 0));

        // A1
        std::array<emp::block, blockSize> tmp {};
        for (size_t i {0}; i != blockSize; ++i) {
            const emp::block& prodXMacOfY {x[i] ? yMac : _mm_set_epi64x(0, 0)};
            emp::block prodYMacOfX;
            emp::gfmul(yValue, x.get_mac(0, i), &prodYMacOfX);
            tmp[i] = _mm_xor_si128(z.get_mac(0, i), prodXMacOfY);
            tmp[i] = _mm_xor_si128(tmp[i], prodYMacOfX);
        }
        emp::block A1 {vector_inner_product<blockSize>(challenges.data(), tmp.data())};
        A1 = _mm_xor_si128(A1, authedBlock.get_block(0));

        io.send_data(&A0, sizeof(A0));
        io.send_data(&A1, sizeof(A1));
    }

    /**
     * verify x[i] * y = z[i]
     * @param x size must be blockSize
     * @param y size must be 1, with only one global key
     * @param z size must be blockSize, with only one global key
     * @throw std::runtime_error if failed.
     */
    template <size_t blockSize>
    void verify(
        ATLab::NetIO& io,
        const BlockCorrelatedOT::Sender& bCOTSender,
        const ITMacBitKeys& x,
        const ITMacBlockKeys& y,
        const ITMacBlockKeys& z
    ) {
        const emp::block delta  {x.get_global_key(0)};
        const auto& yKey {y.get_local_key(0, 0)};
#ifdef DEBUG
        if (as_uint128(delta) != as_uint128(y.get_global_key(0)) ||
            as_uint128(delta) != as_uint128(z.get_global_key(0))
        ) {
            throw std::invalid_argument{"x, y and z do not share the same global key."};
        }
#endif // DEBUG

        const auto authedBlockKey {ITMacBlockKeys{bCOTSender, 1}};

        const emp::block challengeSeed {_mm_set_epi64x(THE_GLOBAL_PRNG(), THE_GLOBAL_PRNG())};
        io.send_data(&challengeSeed, sizeof(challengeSeed));

        auto chalGen {emp::PRG{&challengeSeed}};

        std::array<emp::block, blockSize> challenges;
        chalGen.random_block(challenges.data(), blockSize);

        std::array<emp::block, blockSize> tmp {};
        for (size_t i {0}; i != blockSize; ++i) {
            emp::block prodTwoKeys, prodZWithDelta;
            emp::gfmul(x.get_local_key(0, i), yKey, &prodTwoKeys);
            emp::gfmul(z.get_local_key(0, i), delta, &prodZWithDelta);
            tmp[i] = _mm_xor_si128(prodTwoKeys, prodZWithDelta);
        }

        auto B {vector_inner_product<blockSize>(challenges.data(), tmp.data())};
        B = _mm_xor_si128(B, authedBlockKey.get_local_key(0, 0));

        emp::block A0, A1;
        io.recv_data(&A0, sizeof(A0));
        io.recv_data(&A1, sizeof(A1));

        B = _mm_xor_si128(B, A0);
        emp::block expected;
        emp::gfmul(A1, delta, &expected);
        if (emp::cmpBlock(&B, &expected, 1) == false) {
            throw std::runtime_error{"Malicious behavior detected: DVZK verification failed."};
        }
    }

    inline void prove(
        ATLab::NetIO& io,
        const BlockCorrelatedOT::Receiver& bCOTReceiver,
        const ITMacBits& x,
        const ITMacBlocks& y,
        const ITMacBlocks& z,
        const size_t blockSize
    ) noexcept {
        assert(x.size() == blockSize);
        assert(z.size() == blockSize);
        assert(y.size() == 1);

        const emp::block& yValue {y.get_block(0)};
        const emp::block& yMac {y.get_mac(0, 0)};

        const auto authedBlock {ITMacBlocks{bCOTReceiver, 1}};

        emp::block challengeSeed;
        io.recv_data(&challengeSeed, sizeof(challengeSeed));

        auto chalGen {emp::PRG{&challengeSeed}};
        std::vector<emp::block> challenges(blockSize);
        chalGen.random_block(challenges.data(), blockSize);

        std::vector<emp::block> macProd(blockSize);
        for (size_t i {0}; i != blockSize; ++i) {
            emp::gfmul(x.get_mac(0, i), yMac, &macProd[i]);
        }
        emp::block A0;
        emp::vector_inn_prdt_sum_red(&A0, challenges.data(), macProd.data(), blockSize);
        xor_to(A0, authedBlock.get_mac(0, 0));

        std::vector<emp::block> tmp(blockSize);
        for (size_t i {0}; i != blockSize; ++i) {
            const emp::block prodXMacOfY {x[i] ? yMac : zero_block()};
            emp::block prodYMacOfX;
            emp::gfmul(yValue, x.get_mac(0, i), &prodYMacOfX);
            tmp[i] = z.get_mac(0, i);
            xor_to(tmp[i], prodXMacOfY);
            xor_to(tmp[i], prodYMacOfX);
        }
        emp::block A1;
        emp::vector_inn_prdt_sum_red(&A1, challenges.data(), tmp.data(), blockSize);
        xor_to(A1, authedBlock.get_block(0));

        io.send_data(&A0, sizeof(A0));
        io.send_data(&A1, sizeof(A1));
    }

    inline void verify(
        ATLab::NetIO& io,
        const BlockCorrelatedOT::Sender& bCOTSender,
        const ITMacBitKeys& x,
        const ITMacBlockKeys& y,
        const ITMacBlockKeys& z,
        const size_t blockSize
    ) {
        assert(x.size() == blockSize);
        assert(z.size() == blockSize);
        assert(y.size() == 1);

        const emp::block delta  {x.get_global_key(0)};
        const auto& yKey {y.get_local_key(0, 0)};
        assert(as_uint128(delta) == as_uint128(y.get_global_key(0)));
        assert(as_uint128(delta) == as_uint128(z.get_global_key(0)));

        const auto authedBlockKey {ITMacBlockKeys{bCOTSender, 1}};

        const emp::block challengeSeed {_mm_set_epi64x(THE_GLOBAL_PRNG(), THE_GLOBAL_PRNG())};
        io.send_data(&challengeSeed, sizeof(challengeSeed));

        auto chalGen {emp::PRG{&challengeSeed}};

        std::vector<emp::block> challenges(blockSize);
        chalGen.random_block(challenges.data(), blockSize);

        std::vector<emp::block> tmp(blockSize);
        for (size_t i {0}; i != blockSize; ++i) {
            emp::block prodTwoKeys, prodZWithDelta;
            emp::gfmul(x.get_local_key(0, i), yKey, &prodTwoKeys);
            emp::gfmul(z.get_local_key(0, i), delta, &prodZWithDelta);
            tmp[i] = prodTwoKeys;
            xor_to(tmp[i], prodZWithDelta);
        }

        emp::block B;
        emp::vector_inn_prdt_sum_red(&B, challenges.data(), tmp.data(), blockSize);
        xor_to(B, authedBlockKey.get_local_key(0, 0));

        emp::block A0, A1;
        io.recv_data(&A0, sizeof(A0));
        io.recv_data(&A1, sizeof(A1));

        xor_to(B, A0);
        emp::block expected;
        emp::gfmul(A1, delta, &expected);
        if (emp::cmpBlock(&B, &expected, 1) == false) {
            throw std::runtime_error{"Malicious behavior detected: DVZK verification failed."};
        }
    }
}

#endif // ATLab_DVZK_HPP

#ifndef ATLab_DVZK_HPP
#define ATLab_DVZK_HPP

#include <emp-tool/io/net_io_channel.h>

#include "utils.hpp"
#include "block_correlated_OT.hpp"
#include "authed_bit.hpp"

namespace ATLab::DVZK {
    // proving x[i] y[i] = z[i]
    template<size_t blockSize>
    void prove(
        emp::NetIO& io,
        BlockCorrelatedOT::Receiver& bCOTReceiver,
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
        emp::NetIO& io,
        BlockCorrelatedOT::Sender& bCOTSender,
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

        const auto challengeSeed {ATLab::as_block(PRNG_Kyber::get_PRNG_Kyber()())};
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
}

#endif // ATLab_DVZK_HPP

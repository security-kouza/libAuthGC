#include "ATLab/util_protocols.hpp"

#include "../include/ATLab/params.hpp"
#include "../include/ATLab/PRNG.hpp"
#include "emp-tool/utils/crh.h"
#include "emp-tool/utils/hash.h"

namespace ATLab {
    /**
     * Send the lower 128 bits to the other party and receive the higher 128 bits
     * Then compare the higher bits. Return `true` if the check passes.
     */
    bool compare_hash_high(ATLab::NetIO& io, const void* data, const size_t bytes) {
        const emp::block hashRes {emp::Hash::hash_for_block(data, bytes)};
        const int64_t low = _mm_cvtsi128_si64(hashRes);
        const int64_t high = _mm_extract_epi64(hashRes, 1);

        io.send_data(&low, sizeof(low));
        int64_t receivedHigh;
        io.recv_data(&receivedHigh, sizeof(receivedHigh));

        return high == receivedHigh;
    }

    /**
     * Send the higher 128 bits to the other party and receive the lower 128 bits
     * Then compare the higher bits. Return `true` if the check passes.
     */
    bool compare_hash_low(ATLab::NetIO& io, const void* data, const size_t bytes) {
        const emp::block hashRes {emp::Hash::hash_for_block(data, bytes)};
        const int64_t low = _mm_cvtsi128_si64(hashRes);
        const int64_t high = _mm_extract_epi64(hashRes, 1);

        io.send_data(&high, sizeof(high));
        int64_t receivedLow;
        io.recv_data(&receivedLow, sizeof(receivedLow));

        return low == receivedLow;
    }

    emp::block toss_random_block(ATLab::NetIO& io) {
        emp::block block;
        THE_GLOBAL_PRNG.random_block(&block, 1);

        emp::CRH crh;
        const emp::block cm {crh.H(block)};
        io.send_data(&cm, sizeof(cm));

        emp::block blk_, cm_;
        io.recv_data(&cm_, sizeof(cm_));
        io.send_data(&block, sizeof(block));
        io.recv_data(&blk_, sizeof(blk_));

        if (as_uint128(cm_) != as_uint128(crh.H(blk_))) {
            throw std::runtime_error{ERR_MALICIOUS};
        }
        xor_to(block, blk_);

        return block;
    }
}

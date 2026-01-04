#include "ATLab/util_protocols.hpp"

#include "emp-tool/utils/hash.h"

namespace ATLab {
    /**
     * Send the lower 128 bits to the other party and receive the higher 128 bits
     * Then compare the higher bits. Return `true` if the check passes.
     */
    bool compare_hash_high(emp::NetIO& io, const void* data, const size_t bytes) {
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
    bool compare_hash_low(emp::NetIO& io, const void* data, const size_t bytes) {
        const emp::block hashRes {emp::Hash::hash_for_block(data, bytes)};
        const int64_t low = _mm_cvtsi128_si64(hashRes);
        const int64_t high = _mm_extract_epi64(hashRes, 1);

        io.send_data(&high, sizeof(high));
        int64_t receivedLow;
        io.recv_data(&receivedLow, sizeof(receivedLow));

        return low == receivedLow;
    }
}

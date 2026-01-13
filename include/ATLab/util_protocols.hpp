#ifndef ATLab_UTIL_PROTOCOLS_HPP
#define ATLab_UTIL_PROTOCOLS_HPP

#include <cstddef>

#include "ATLab/net-io.hpp"

namespace ATLab {
    bool compare_hash_high(NetIO& io, const void* data, size_t bytes);
    bool compare_hash_low(NetIO& io, const void* data, size_t bytes);

    // Jointly sample a random block.
    // No paired function. Both parties can call this at the same time.
    emp::block toss_random_block(NetIO& io);
}

#endif // ATLab_UTIL_PROTOCOLS_HPP

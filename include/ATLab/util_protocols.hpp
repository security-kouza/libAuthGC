#ifndef ATLab_UTIL_PROTOCOLS_HPP
#define ATLab_UTIL_PROTOCOLS_HPP

#include <cstddef>

#include "emp-tool/io/net_io_channel.h"

namespace ATLab {
    bool compare_hash_high(emp::NetIO& io, const void* data, const size_t bytes);
    bool compare_hash_low(emp::NetIO& io, const void* data, const size_t bytes);
}

#endif // ATLab_UTIL_PROTOCOLS_HPP

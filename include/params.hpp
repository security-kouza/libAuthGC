#ifndef ATLab_REVELIO_PARAMS_HPP
#define ATLab_REVELIO_PARAMS_HPP

#include <cstddef>

namespace ATLab {
    constexpr size_t STATISTICAL_SECURITY {40};
    constexpr size_t BLOCK_BIT_SIZE {128}; // number of bits in __m128i. DO NOT CHANGE THIS!
    constexpr size_t DIGEST_SIZE {32}, HALF_DIGEST_SIZE {16}; // SHA-256 digest size
}

#endif // ATLab_REVELIO_PARAMS_HPP

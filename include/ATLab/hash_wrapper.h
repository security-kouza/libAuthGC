#ifndef ATLAB_HASH_WRAPPER_H
#define ATLAB_HASH_WRAPPER_H

#include <array>

#include <emp-tool/utils/hash.h>

namespace ATLab {
    namespace SHA256 {
        inline std::array<std::byte, 16> hash_to_128 (const void* data, const size_t size) {
            const emp::block hashBlock {emp::Hash::hash_for_block(data, size)};
            std::array<std::byte, 16> res;
            std::memcpy(res.data(), &hashBlock, sizeof(res));
            return res;
        }
    }
}

#endif // ATLAB_HASH_WRAPPER_H

#ifndef ATLAB_TRAITS_HPP
#define ATLAB_TRAITS_HPP

#include <cstddef>
#include <array>
#include <type_traits>

namespace ATLab {
    template <typename>
    struct is_valid_byte_array: std::false_type {};

    template <size_t N>
    struct is_valid_byte_array<std::array<std::byte, N>>: std::bool_constant<(N > 0)> {};

    template <typename, typename = void>
    struct is_valid_hash: std::false_type {};

    template <typename HashF>
    struct is_valid_hash<HashF, std::void_t<
        std::invoke_result_t<HashF, const void*, size_t>
    >>: is_valid_byte_array<std::invoke_result_t<HashF, const void*, size_t>> {};

    namespace detail {
        template <typename T, bool valid = is_valid_hash<T>::value>
        struct HashResCheck {
            static_assert(valid, "HashF must be a valid hash function.");
            using type = void;
        };

        template <typename HashF>
        struct HashResCheck<HashF, true> {
            using type = std::invoke_result_t<HashF, const void*, size_t>;
        };
    }

    template <typename HashF>
    using HashRes = typename detail::HashResCheck<HashF>::type;

    template <typename HashF>
    constexpr size_t hash_byte() {
        return std::tuple_size_v<HashRes<HashF>>;
    }
}

#endif // ATLAB_TRAITS_HPP

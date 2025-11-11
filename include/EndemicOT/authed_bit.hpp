#ifndef ATLab_AUTHED_BIT_HPP
#define ATLab_AUTHED_BIT_HPP

#include <vector>

#include <emp-tool/utils/block.h>

#include "block_correlated_OT.hpp"

namespace ATLab {
    class ITMacBits {
        std::vector<bool> _bits;
        std::vector<emp::block> _macs;

    public:
        ITMacBits() = delete;
        ITMacBits(std::vector<bool>&& bits, std::vector<emp::block>&& macs) :
            _bits {std::move(bits)},
            _macs {std::move(macs)}
        {}
    };

    class ITMacKeys {
        std::vector<emp::block> _localKeys;
        emp::block _globalKey;
    public:
    };

}

#endif // ATLab_AUTHED_BIT_HPP

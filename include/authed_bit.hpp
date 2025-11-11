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
        ITMacBits(std::vector<bool>&& bits, std::vector<emp::block>&& macs) noexcept:
            _bits {std::move(bits)},
            _macs {std::move(macs)}
        {}

        /**
         * Random ITMacBit constructor
         * Protocol interactions involved.
         * @param bCOTreceiver a bCOT receiver with only one delta.
         * @param len Number of bits to generate.
         */
        ITMacBits(BlockCorrelatedOT::Receiver& bCOTreceiver, const size_t len):
            ITMacBits {[&bCOTreceiver, len]() -> ITMacBits {
                if (bCOTreceiver.deltaArrSize != 1) {
                    throw std::runtime_error("Only support one delta for bCOT bCOTreceiver.");
                }
                auto [u, m] {bCOTreceiver.extend(len)};
                return ITMacBits{std::move(u), std::move(m)};
            }()}
        {}
    };

    class ITMacKeys {
        std::vector<emp::block> _localKeys;
        emp::block _globalKey;
    public:
        ITMacKeys() = delete;
        ITMacKeys(std::vector<emp::block>&& localKeys, const emp::block globalKey) noexcept:
            _localKeys {std::move(localKeys)},
            _globalKey {globalKey}
        {}

        /**
         * Construct keys for random authenticated bits
         * Protocol interactions involved.
         * @param bCOTsender a bCOT sender with only one delta.
         * @param len Number of bits to generate.
         */
        ITMacKeys(BlockCorrelatedOT::Sender& bCOTsender, const size_t len):
            ITMacKeys {[&bCOTsender, len]() -> ITMacKeys {
                if (bCOTsender.deltaArrSize != 1) {
                    throw std::runtime_error("Only support one delta for bCOT bCOTsender.");
                }
                return ITMacKeys {
                    bCOTsender.extend(len),
                    bCOTsender.get_delta(0)
                };
            }()}
        {}
    };

}

#endif // ATLab_AUTHED_BIT_HPP

#include <authed_bit.hpp>

namespace ATLab {
    namespace {
        std::vector<bool> blocks_to_bool_vector(const std::vector<emp::block>& blocks) {
            std::vector<bool> bits;
            bits.reserve(blocks.size() * BLOCK_BIT_SIZE);
            for (const auto& block : blocks) {
                const auto blockBits {to_bool_vector(block)};
                bits.insert(bits.end(), blockBits.begin(), blockBits.end());
            }
            return bits;
        }
    }

    ITMacBlocks::ITMacBlocks(
        emp::NetIO& io,
        BlockCorrelatedOT::Receiver& bCOTReceiver,
        std::vector<emp::block> blocksToAuth
    ):
        ITMacBlocks {
            ITMacBits{io, bCOTReceiver, blocks_to_bool_vector(blocksToAuth)}.polyval_to_Blocks()
        }
    {}

    ITMacBlockKeys::ITMacBlockKeys(
        emp::NetIO& io,
        BlockCorrelatedOT::Sender& bCOTSender,
        size_t blockCount
    ):
        ITMacBlockKeys {
            [&io, &bCOTSender, blockCount]() -> ITMacBlockKeys {
                if (!blockCount) {
                    throw std::invalid_argument{"blockCount cannot be zero."};
                }
                return ITMacBitKeys{io, bCOTSender, blockCount * BLOCK_BIT_SIZE}.polyval_to_Blocks();
            }()
        }
    {}


}

#include <authed_bit.hpp>

namespace ATLab {
    ITMacBlocks::ITMacBlocks(
        emp::NetIO& io,
        BlockCorrelatedOT::Receiver& bCOTReceiver,
        const emp::block& blockToAuth
    ):
        ITMacBlocks {ITMacBits{io, bCOTReceiver, to_bool_vector(blockToAuth)}.polyval_to_Blocks()}
    {}

    ITMacBlockKeys::ITMacBlockKeys(emp::NetIO& io, BlockCorrelatedOT::Sender& bCOTSender):
        ITMacBlockKeys {ITMacBitKeys{io, bCOTSender, 128}.polyval_to_Blocks()}
    {}


}

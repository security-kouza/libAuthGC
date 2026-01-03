#include <bitset>

#include "authed_bit.hpp"

namespace ATLab {
    namespace {
        Bitset blocks_to_bool_vector(const std::vector<emp::block>& blocks) {
            const size_t totalBits {blocks.size() * BLOCK_BIT_SIZE};
            Bitset bits(totalBits);
            size_t offset {0};
            for (const auto& block : blocks) {
                const auto blockBits {to_bool_vector(block)};
                for (size_t bitIter {0}; bitIter != BLOCK_BIT_SIZE; ++bitIter) {
                    bits[offset + bitIter] = blockBits[bitIter];
                }
                offset += BLOCK_BIT_SIZE;
            }
            return bits;
        }

        std::vector<emp::block> bits_to_blocks(const Bitset& bits) {
            std::vector<emp::block> blocks;
            const size_t blockCount {bits.size() / BLOCK_BIT_SIZE};
            blocks.reserve(blockCount);
            std::bitset<BLOCK_BIT_SIZE> bitChunk;
            for (size_t blockIter {0}; blockIter != blockCount; ++blockIter) {
                for (size_t bitIter {0}; bitIter != BLOCK_BIT_SIZE; ++bitIter) {
                    bitChunk[bitIter] = bits.test(blockIter * BLOCK_BIT_SIZE + bitIter);
                }
                blocks.push_back(Block(bitChunk));
            }
            return blocks;
        }

        std::vector<emp::block> polyval_mac_chunks(
            const emp::block* macs,
            const size_t totalBitsPerKey,
            const size_t globalKeySize,
            const size_t blockCount
        ) {
            std::vector<emp::block> blockMacs;
            blockMacs.reserve(blockCount * globalKeySize);
            for (size_t deltaIter {0}; deltaIter != globalKeySize; ++deltaIter) {
                const emp::block* macBase {macs + deltaIter * totalBitsPerKey};
                for (size_t blockIter {0}; blockIter != blockCount; ++blockIter) {
                    blockMacs.push_back(polyval(macBase + blockIter * BLOCK_BIT_SIZE));
                }
            }
            return blockMacs;
        }

        Bitset scaled_block_bits(const emp::block& scalarBlock, const Bitset& selectors) {
            const size_t blockCount {selectors.size()};
            Bitset bits(blockCount * BLOCK_BIT_SIZE);
            if (!selectors.any()) {
                return bits;
            }
            const Bitset blockBits {to_bool_vector(scalarBlock)};
            for (auto pos {selectors.find_first()}; pos != Bitset::npos; pos = selectors.find_next(pos)) {
                const size_t offset {pos * BLOCK_BIT_SIZE};
                for (size_t bitIter {0}; bitIter != BLOCK_BIT_SIZE; ++bitIter) {
                    if (blockBits[bitIter]) {
                        bits.set(offset + bitIter);
                    }
                }
            }
            return bits;
        }
    }

    ITMacBlocks::ITMacBlocks(
        std::vector<emp::block>&& blocks,
        std::vector<emp::block>&& macs,
        const size_t globalKeySize
    ):
        _macs {std::move(macs)},
        _blocks {std::move(blocks)},
        _globalKeySize {globalKeySize}
    {
        if (_blocks.empty() || !_globalKeySize) {
            throw std::invalid_argument{"Empty argument"};
        }
        if (_macs.size() != _blocks.size() * _globalKeySize) {
            throw std::invalid_argument{"Wrong parameter sizes."};
        }
    }

    ITMacBlocks::ITMacBlocks(
        const Bitset& bits,
        std::vector<emp::block> macs,
        const size_t deltaArrSize
    ):
        ITMacBlocks {
            [&bits, &macs, deltaArrSize]() {
                const size_t totalBits {bits.size()};
                if (!totalBits || totalBits % BLOCK_BIT_SIZE != 0) {
                    throw std::invalid_argument{"Wrong parameter sizes."};
                }
                if (macs.size() != totalBits * deltaArrSize) {
                    throw std::invalid_argument{"Wrong parameter sizes."};
                }

                const size_t blockCount {totalBits / BLOCK_BIT_SIZE};
                auto blocks {bits_to_blocks(bits)};
                auto blockMacs {polyval_mac_chunks(macs.data(), totalBits, deltaArrSize, blockCount)};

                return ITMacBlocks{std::move(blocks), std::move(blockMacs), deltaArrSize};
            }()
        }
    {}

    ITMacBlocks::ITMacBlocks(
        const BlockCorrelatedOT::Receiver& bCOTReceiver,
        const size_t blockSize
    ):
        ITMacBlocks {
            [&bCOTReceiver, blockSize]() {
                if (!blockSize) {
                    throw std::invalid_argument{"blockSize cannot be zero."};
                }
                return ITMacBits{bCOTReceiver, blockSize * BLOCK_BIT_SIZE}.polyval_to_Blocks();
            }()
        }
    {}

    ITMacBlocks::ITMacBlocks(
        emp::NetIO& io,
        BlockCorrelatedOT::Receiver& bCOTReceiver,
        std::vector<emp::block> blocksToAuth
    ):
        ITMacBlocks {
            [&io, &bCOTReceiver, blocks = std::move(blocksToAuth)]() mutable {
                auto bitsToFix {blocks_to_bool_vector(blocks)};
                const ITMacBits fixedBits {io, bCOTReceiver, std::move(bitsToFix)};

                const size_t totalBits {fixedBits.size()};
                if (!totalBits || totalBits % BLOCK_BIT_SIZE != 0) {
                    throw std::invalid_argument{"Wrong parameter sizes."};
                }

                const size_t blockCount {blocks.size()};
                if (blockCount * BLOCK_BIT_SIZE != totalBits) {
                    throw std::invalid_argument{"Wrong parameter sizes."};
                }

                const size_t globalKeySize {fixedBits.global_key_size()};
                auto blockMacs {polyval_mac_chunks(fixedBits._macs.data(), totalBits, globalKeySize, blockCount)};

                return ITMacBlocks{std::move(blocks), std::move(blockMacs), globalKeySize};
            }()
        }
    {}

    ITMacScaledBits::ITMacScaledBits(
        emp::NetIO& io,
        BlockCorrelatedOT::Receiver& bCOTReceiver,
        const emp::block& scalarBlock,
        const Bitset& blockSelectors
    ):
        _selectors {blockSelectors},
        _scalar {scalarBlock}
    {
        const size_t blockCount {_selectors.size()};
        if (!blockCount) {
            throw std::invalid_argument{"blockSelectors cannot be empty."};
        }

        auto bitsToFix {scaled_block_bits(scalarBlock, _selectors)};
        const ITMacBits fixedBits {io, bCOTReceiver, std::move(bitsToFix)};

        const size_t totalBits {fixedBits.size()};
        if (!totalBits || totalBits % BLOCK_BIT_SIZE != 0) {
            throw std::invalid_argument{"Wrong parameter sizes."};
        }

        if (blockCount * BLOCK_BIT_SIZE != totalBits) {
            throw std::invalid_argument{"Wrong parameter sizes."};
        }

        const size_t globalKeySize {fixedBits.global_key_size()};
        if (globalKeySize != 1) {
            throw std::invalid_argument{"ITMacScaledBits only supports a single global key."};
        }

        _macs = polyval_mac_chunks(fixedBits._macs.data(), totalBits, globalKeySize, blockCount);
    }

    ITMacBlockKeys::ITMacBlockKeys(
        const BlockCorrelatedOT::Sender& bCOTSender,
        size_t blockSize
    ):
        ITMacBlockKeys {
            [&bCOTSender, blockSize]() -> ITMacBlockKeys {
                if (!blockSize) {
                    throw std::invalid_argument{"blockSize cannot be zero."};
                }
                return ITMacBitKeys{bCOTSender, blockSize * BLOCK_BIT_SIZE}.polyval_to_Blocks();
            }()
        }
    {}

    ITMacBlockKeys::ITMacBlockKeys(
        emp::NetIO& io,
        BlockCorrelatedOT::Sender& bCOTSender,
        size_t blockSize
    ):
        ITMacBlockKeys {
            [&io, &bCOTSender, blockSize]() -> ITMacBlockKeys {
                if (!blockSize) {
                    throw std::invalid_argument{"blockCount cannot be zero."};
                }
                return ITMacBitKeys{io, bCOTSender, blockSize * BLOCK_BIT_SIZE}.polyval_to_Blocks();
            }()
        }
    {}


}


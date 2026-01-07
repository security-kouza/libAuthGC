#include <bitset>

#include "authed_bit.hpp"

#include "ATLab/hash_wrapper.h"
#include "ATLab/util_protocols.hpp"

namespace ATLab {
    namespace {
        Bitset blocks_to_bool_vector(const boost::span<emp::block>& blocks) {
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
    ) noexcept:
        _macs {std::move(macs)},
        _blocks {std::move(blocks)},
        _globalKeySize {globalKeySize}
    {
        assert(!_blocks.empty());
        assert(_globalKeySize != 0);
        assert(_macs.size() == _blocks.size() * _globalKeySize);
    }

    ITMacBlocks::ITMacBlocks(
        const boost::span<const emp::block> blocks,
        const boost::span<const emp::block> macs,
        const size_t globalKeySize
    ) noexcept:
        _globalKeySize {globalKeySize}
    {
        assert(!blocks.empty());
        assert(_globalKeySize != 0);
        assert(macs.size() == blocks.size() * _globalKeySize);

        _blocks.reserve(blocks.size());
        _macs.reserve(macs.size());

        std::copy_n(blocks.cbegin(), blocks.size(), std::back_inserter(_blocks));
        std::copy_n(macs.cbegin(), macs.size(), std::back_inserter(_macs));
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
        const BlockCorrelatedOT::Receiver& bCOTReceiver,
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

    ITMacBlocks::ITMacBlocks(
        emp::NetIO& io,
        const BlockCorrelatedOT::Receiver& bCOTReceiver,
        boost::span<const emp::block> blocksToAuth
    ):
        ITMacBlocks {
            [&io, &bCOTReceiver, blocksToAuth]() {
                std::vector<emp::block> blockVec;
                blockVec.reserve(blocksToAuth.size());
                std::copy_n(blocksToAuth.cbegin(), blocksToAuth.size(), std::back_inserter(blockVec));

                return ITMacBlocks{io, bCOTReceiver, blockVec};
            }()
        }
    {}

    ITMacBlockKeys ITMacBlocks::swap_value_and_key(const size_t globalKeyPos, const size_t blockPos) const noexcept {
        const auto& newGlobalKey {get_block(blockPos)};
        const auto& newLocalKey {get_mac(globalKeyPos, blockPos)};

        return {{newLocalKey}, newGlobalKey};
    }

    ITMacBlockKeys ITMacBlocks::swap_value_and_key() && noexcept {
        assert(size() == 1);

        return {std::move(_macs), _blocks.front()};
    }

    void ITMacBlocks::inverse_value_and_mac() noexcept {
        std::swap(_macs, _blocks);
    }

    ITMacScaledBits::ITMacScaledBits(
        emp::NetIO& io,
        const BlockCorrelatedOT::Receiver& bCOTReceiver,
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

    // Constructor for a random block
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

    // Constructor for a fixed block
    ITMacBlockKeys::ITMacBlockKeys(
        emp::NetIO& io,
        const BlockCorrelatedOT::Sender& bCOTSender,
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

    ITMacBlocks ITMacBlockKeys::swap_value_and_key(const size_t globalKeyPos, const size_t blockPos) const noexcept {
        const emp::block newBlock {get_global_key(globalKeyPos)}, newMac {get_local_key(globalKeyPos, blockPos)};
        return {{newBlock}, {newMac}, 1};
    }

    ITMacBlocks ITMacBlockKeys::swap_value_and_key() && noexcept {
        assert(size() == 1);
        return {std::move(_globalKeys), std::move(_localKeys), 1};
    }

    void ITMacBlockKeys::inverse_value_and_mac() noexcept {
        for (size_t i {0}; i != global_key_size(); ++i) {
            _globalKeys[i] = gf_inverse(_globalKeys[i]);
            boost::span localKeys {_localKeys.data() + i * size(), size()};
            for (emp::block& key : localKeys) {
                key = gf_mul_block(key, _globalKeys[i]);
            }
        }
    }

    bool check_same_bit(emp::NetIO& io, const ITMacBlockKeySpan& key0, const ITMacBlockKeySpan& key1) noexcept {
        assert(key0.size() == 1);
        assert(key1.size() == 1);
        const emp::block toCompare {_mm_xor_si128(key0.get_local_key(0), key1.get_local_key(0))};
        return compare_hash_high(io, &toCompare, sizeof(toCompare));
    }

    bool check_same_bit(emp::NetIO& io, const ITMacBlockSpan& block0, const ITMacBlockSpan& block1) noexcept {
        assert(block0.size() == 1);
        assert(block1.size() == 1);
        const emp::block toCompare {_mm_xor_si128(block0.get_mac(0), block1.get_mac(0))};
        return compare_hash_low(io, &toCompare, sizeof(toCompare));
    }

    void eqcheck_diff_key(
        emp::NetIO& io,
        const ITMacBlockSpan& authedBlocks0,
        const ITMacBlockSpan& authedBlocks1
    ) noexcept {
        assert(authedBlocks0.size() == authedBlocks1.size());
        const size_t size {authedBlocks0.size()};

        const BlockCorrelatedOT::Receiver bCOTKey0 {io, 1};
        const ITMacBlocks authedMacs1 {io, bCOTKey0, authedBlocks1.mac_span()};

        const BlockCorrelatedOT::Receiver bCOTKey1 {io, 1};
        const ITMacBlocks authedMacs0 {io, bCOTKey1, authedBlocks0.mac_span()};

        std::vector<ITMacBlocks::MacType> toHash;
        toHash.reserve(size);
        for (size_t i = 0; i < size; i++) {
            toHash.push_back(_mm_xor_si128(authedMacs0.get_mac(0, i), authedMacs1.get_mac(0, i)));
        }
        const auto hashRes {SHA256::hash_to_128(toHash.data(), toHash.size())};
        io.send_data(&hashRes, sizeof(hashRes));
    }

    void eqcheck_diff_key(
        emp::NetIO& io,
        const ITMacBlockKeySpan& authedBlocks0,
        const ITMacBlockKeySpan& authedBlocks1
    ) {
        assert(authedBlocks0.size() == authedBlocks1.size());
        const size_t size {authedBlocks0.size()};
        const auto& gk0 {authedBlocks0.global_key()}, gk1 {authedBlocks1.global_key()};

        const BlockCorrelatedOT::Sender bCOTKey0 {io, {authedBlocks0.global_key()}};
        const ITMacBlockKeys authedMacs1 {io, bCOTKey0, size};

        const BlockCorrelatedOT::Sender bCOTKey1 {io, {authedBlocks1.global_key()}};
        const ITMacBlockKeys authedMacs0 {io, bCOTKey1, size};

        std::vector<ITMacBlocks::MacType> toHash;
        toHash.reserve(size);
        for (size_t i = 0; i < size; i++) {
            emp::block newTerm {
                _mm_xor_si128(authedMacs0.get_local_key(0, i), authedMacs1.get_local_key(0, i))
            };
            emp::block tmp;
            emp::gfmul(authedBlocks0.get_local_key(i), gk1, &tmp);
            xor_to(newTerm, tmp);
            emp::gfmul(authedBlocks1.get_local_key(i), gk0, &tmp);
            xor_to(newTerm, tmp);
            toHash.push_back(newTerm);
        }
        const auto hashRes {SHA256::hash_to_128(toHash.data(), toHash.size())};
        std::array<std::byte, 16> expectedHash {};
        io.recv_data(&expectedHash, sizeof(expectedHash));

        if (hashRes != expectedHash) {
            throw std::runtime_error{"Malicious behavior detected."};
        }
    }

}


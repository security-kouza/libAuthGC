#ifndef ATLab_AUTHED_BIT_HPP
#define ATLab_AUTHED_BIT_HPP

#include <array>
#include <stdexcept>
#include <vector>

#include <emp-tool/utils/block.h>

#include "params.hpp"
#include "block_correlated_OT.hpp"
#include "ATLab/matrix.hpp"

namespace ATLab {
    class ITMacBlocks {
        std::vector<emp::block> _macs; // flattened by global key then block index
        std::vector<emp::block> _blocks;
        const size_t _globalKeySize; // number of global keys

        ITMacBlocks(std::vector<emp::block>&& blocks, std::vector<emp::block>&& macs, size_t globalKeySize);

    public:

        /**
         *
         * @param bits size must be 128 * blockSize
         * @param macs size must be bits.size() * deltaArrSize
         */
        ITMacBlocks(const Bitset& bits, std::vector<emp::block> macs, size_t deltaArrSize);

        /**
         * Random ITMacBlock constructor
         * Protocol interactions involved.
         * @param blockSize Number of blocks to generate.
         */
        ITMacBlocks(BlockCorrelatedOT::Receiver& bCOTReceiver, size_t blockSize);

        // `Fix` for blocks
        ITMacBlocks(emp::NetIO&, BlockCorrelatedOT::Receiver&, std::vector<emp::block> blocksToAuth);

        size_t global_key_size() const {
            return _globalKeySize;
        }

        size_t size() const {
            return _blocks.size();
        }

        const emp::block& get_block(const size_t blockPos = 0) const {
            return _blocks.at(blockPos);
        }

        const emp::block& get_mac(const size_t globalKeyPos, const size_t blockPos = 0) const {
            return _macs.at(globalKeyPos * size() + blockPos);
        }

        const std::vector<emp::block>& get_all_macs() const {
            return _macs;
        }

        // block ^= 1, macs unchanged
        void flip_block_lsb(const size_t blockPos = 0) {
            _blocks.at(blockPos) = _mm_xor_si128(_blocks.at(blockPos), _mm_set_epi64x(0, 1));
        }

        std::vector<emp::block> release_macs() && {
            return std::move(_macs);
        }

        friend ITMacBlocks operator*(const Matrix<bool>&, const ITMacBlocks&);
    };

    class ITMacScaledBits {
        Bitset _selectors;
        std::vector<emp::block> _macs;
        emp::block _scalar;

    public:
        ITMacScaledBits(
            emp::NetIO&,
            BlockCorrelatedOT::Receiver&,
            const emp::block& scalarBlock,
            const Bitset& blockSelectors
        );

        size_t size() const {
            return _selectors.size();
        }

        const Bitset& selectors() const {
            return _selectors;
        }

        const emp::block& scalar() const {
            return _scalar;
        }

        emp::block get_block(const size_t blockPos = 0) const {
            return _selectors.test(blockPos) ? _scalar : emp::zero_block;
        }

        const emp::block& get_mac(const size_t blockPos = 0) const {
            return _macs.at(blockPos);
        }

        size_t global_key_size() const {
            return 1;
        }
    };

    class ITMacBlockKeys {
        std::vector<emp::block> _localKeys; // flattened by global key then block index
        std::vector<emp::block> _globalKeys;

        // Move localkeys only when globalKey with size 1
        // private constructor, only used by the friend matrix multiplication
        ITMacBlockKeys(std::vector<emp::block>&& localKeys, const emp::block globalKey):
            _localKeys {std::move(localKeys)},
            _globalKeys {globalKey}
        {}
    public:
        ITMacBlockKeys(const std::vector<emp::block>& localKeys, std::vector<emp::block> globalKeys):
            _globalKeys {std::move(globalKeys)}
        {
            if (_globalKeys.empty()) {
                throw std::invalid_argument{"Global keys cannot be empty."};
            }
            if (localKeys.size() % _globalKeys.size() != 0) {
                throw std::invalid_argument{"Wrong parameter sizes."};
            }
            const size_t totalBitsPerGlobalKey {localKeys.size() / _globalKeys.size()};
            if (!totalBitsPerGlobalKey || totalBitsPerGlobalKey % BLOCK_BIT_SIZE != 0) {
                throw std::invalid_argument{"Size of localKeys per global key is not a multiple of 128."};
            }
            const size_t blockSize {totalBitsPerGlobalKey / BLOCK_BIT_SIZE};
            _localKeys.reserve(blockSize * _globalKeys.size());
            for (size_t i {0}; i != _globalKeys.size(); ++i) {
                const emp::block* localBase {localKeys.data() + i * totalBitsPerGlobalKey};
                for (size_t blockIter {0}; blockIter != blockSize; ++blockIter) {
                    _localKeys.push_back(polyval(localBase + blockIter * BLOCK_BIT_SIZE));
                }
            }
        }

        /**
         * Random ITMacBlock key constructor
         * Protocol interactions involved.
         * @param blockSize Number of blocks to generate.
         */
        ITMacBlockKeys(BlockCorrelatedOT::Sender& bCOTSender, size_t blockSize);

        // `Fix` for blocks
        ITMacBlockKeys(emp::NetIO&, BlockCorrelatedOT::Sender&, size_t blockSize);

        size_t global_key_size() const {
            return _globalKeys.size();
        }

        size_t size() const {
            if (_globalKeys.empty()) {
                return 0;
            }
            return _localKeys.size() / _globalKeys.size();
        }

        const emp::block& get_local_key(const size_t globalKeyPos, const size_t blockPos = 0) const {
            return _localKeys.at(globalKeyPos * size() + blockPos);
        }
        const emp::block& get_global_key(const size_t pos) const {
            return _globalKeys.at(pos);
        }

        // adding authenticated 1
        void flip_block_lsb(const size_t blockPos = 0) {
            const size_t blockSize {size()};
            for (size_t i {0}; i != global_key_size(); ++i) {
                _localKeys.at(i * blockSize + blockPos) =
                    _mm_xor_si128(_localKeys.at(i * blockSize + blockPos), _globalKeys.at(i));
            }
        }

        friend ITMacBlockKeys operator*(const Matrix<bool>&, const ITMacBlockKeys&);
    };

    class ITMacBits {
        friend class ITMacBlocks;
        friend class ITMacScaledBits;

        Bitset _bits;
        std::vector<emp::block> _macs; // _macs.size() == _bits.size() * deltaArrSize
    public:
        ITMacBits() = delete;
        ITMacBits(ITMacBits&&) = default;

        /**
         * Direct construction.
         * @param bits Cannot be empty.
         * @param macs Must be a multiple of bits.size()
         */
        ITMacBits(Bitset&& bits, std::vector<emp::block>&& macs):
            _bits {std::move(bits)},
            _macs {std::move(macs)}
        {
#ifdef DEBUG
            if (_bits.empty()) {
                assert(_bits.empty());
            } else if (_macs.size() % _bits.size()) {
                throw std::runtime_error{"The size of macs is not a multiple of the size of bits."};
            }
#endif // DEBUG
        }

        /**
         * Random ITMacBit constructor
         * Protocol interactions involved.
         * @param len Number of bits to generate.
         */
        ITMacBits(BlockCorrelatedOT::Receiver& bCOTReceiver, const size_t len):
            ITMacBits {[&bCOTReceiver, len]() -> ITMacBits {
                auto [u, m] {bCOTReceiver.extend(len)};
                return ITMacBits{std::move(u), std::move(m)};
            }()}
        {}

        /**
         * Fixed ITMacBit constructor, the `Fix` procedure defined in CYYW23.
         * Will invoke the random constructor first, and send to
         */
        ITMacBits(emp::NetIO& io, BlockCorrelatedOT::Receiver& bCOTReceiver, Bitset bitsToFix):
            ITMacBits{bCOTReceiver, bitsToFix.size()}
        {

            // Compute XOR between the generated bits and the bits to fix, store the result in fixedBits
            // TODO: Compact the bool array to save communication
            auto* diffArr = new bool[_bits.size()];
            for (size_t i {0}; i < _bits.size(); ++i) {
                diffArr[i] = _bits[i] ^ bitsToFix[i];
            }
            io.send_data(diffArr, sizeof(bool) * _bits.size());

            _bits = std::move(bitsToFix);
            delete[] diffArr;
        }

        size_t size() const {
            return _bits.size();
        }

        size_t global_key_size() const {
            return _macs.size() / _bits.size();
        }

        // TODO: rename it to test
        bool at(const size_t pos) const {
            return _bits.test(pos);
        }

        bool operator[](const size_t pos) const {
            return _bits[pos];
        }

        const emp::block& get_mac(const size_t globalKeyPos, const size_t bitPos) const {
            return _macs.at(bitPos + globalKeyPos * size());
        }

        const Bitset& bits() const {
            return _bits;
        }

        ITMacBlocks polyval_to_Blocks() && {
            const size_t GLOBAL_KEY_SIZE {global_key_size()};
            return ITMacBlocks{_bits, std::move(_macs), GLOBAL_KEY_SIZE};
        }

        friend ITMacBits operator*(const Matrix<bool>&, const ITMacBits&);
    };

    class ITMacBitKeys {
        std::vector<emp::block> _localKeys;
        std::vector<emp::block> _globalKeys;

    public:
        ITMacBitKeys() = delete;
        ITMacBitKeys(ITMacBitKeys&&) = default;

        ITMacBitKeys(std::vector<emp::block>&& localKeys, std::vector<emp::block> globalKeys):
            _localKeys {std::move(localKeys)},
            _globalKeys {std::move(globalKeys)}
        {
#ifdef DEBUG
            if (_globalKeys.empty()) {
                throw std::runtime_error{"Global keys cannot be empty."};
            }
            if (_localKeys.size() % _globalKeys.size() != 0) {
                throw std::runtime_error{"The localKeys size if not a multiple of the globalKeys size."};
            }
#endif // DEBUG
        }

        /**
         * Construct keys for random authenticated bits
         * Protocol interactions involved.
         * _bits will not be available.
         * @param len Number of bits to generate.
         */
        ITMacBitKeys(BlockCorrelatedOT::Sender& bCOTSender, const size_t len):
            ITMacBitKeys {[&bCOTSender, len]() -> ITMacBitKeys {
                return ITMacBitKeys {
                    bCOTSender.extend(len),
                    bCOTSender.get_delta_arr()
                };
            }()}
        {}

        /**
         * Fixed ITMacKey constructor, the `Fix` procedure defined in CYYW23.
         * @param bitsSize size of bits to be fixed
         */
        ITMacBitKeys(emp::NetIO& io, BlockCorrelatedOT::Sender& bCOTSender, const size_t bitsSize):
            ITMacBitKeys{bCOTSender, bitsSize}
        {
            auto* diffArr {new bool[bitsSize]};
            io.recv_data(diffArr, sizeof(bool) * bitsSize);

            std::vector<emp::block> masks;
            masks.reserve(bitsSize);
            for (size_t i {0}; i < bitsSize; ++i) {
                // false => 0x0000...; true => 0xFFFF....
                masks.push_back(_mm_set1_epi64x(-static_cast<uint64_t>(diffArr[i])));
            }

            for (size_t iterGlobalKey {0}; iterGlobalKey < _globalKeys.size(); ++iterGlobalKey) {
                const emp::block& gk = _globalKeys[iterGlobalKey];
                const size_t rowBase = iterGlobalKey * bitsSize;
                for (size_t iterBit {0}; iterBit < bitsSize; ++iterBit) {
                    emp::block& lk = _localKeys[rowBase + iterBit];
                    lk = _mm_xor_si128(lk, _mm_and_si128(gk, masks[iterBit]));
                }
            }

            delete[] diffArr;
        }

        size_t size() const {
            return _localKeys.size() / _globalKeys.size();
        }

        size_t global_key_size () const {
            return _globalKeys.size();
        }

        const emp::block& get_local_key(const size_t globalKeyPos, const size_t bitPos) const {
            return _localKeys.at(bitPos + globalKeyPos * size());
        }

        const emp::block& get_global_key(const size_t pos) const {
            return _globalKeys.at(pos);
        }

        ITMacBlockKeys polyval_to_Blocks() && {
            return ITMacBlockKeys{_localKeys, std::move(_globalKeys)};
        }

        friend ITMacBitKeys operator*(const Matrix<bool>&, const ITMacBitKeys&);
    };
}

#endif // ATLab_AUTHED_BIT_HPP

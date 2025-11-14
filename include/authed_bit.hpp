#ifndef ATLab_AUTHED_BIT_HPP
#define ATLab_AUTHED_BIT_HPP

#include <vector>

#include <emp-tool/utils/block.h>

#include "utils.hpp"
#include "block_correlated_OT.hpp"

namespace ATLab {
    class ITMacBlocks {
        std::vector<emp::block> _macs;
        const size_t _size; // global key size
        emp::block _block;
    public:

        /**
         *
         * @param bits size must be 128
         * @param macs size must be 128 * deltaArrSize
         */
        ITMacBlocks(const std::vector<bool>& bits, std::vector<emp::block> macs, const size_t deltaArrSize):
            _size(deltaArrSize),
            _block {Block(bits)}
        {
            if (bits.size() != 128 || macs.size() != 128 * deltaArrSize) {
                throw std::invalid_argument{"Wrong parameter sizes."};
            }

            _macs.reserve(_size);
            for (size_t i {0}; i != deltaArrSize; ++i) {
                _macs.push_back(polyval(macs.data() + 128 * i));
            }
        }

        // `Fix` for a block
        ITMacBlocks(emp::NetIO&, BlockCorrelatedOT::Receiver&, const emp::block& blockToAuth);

        size_t global_key_size() const {
            return _size;
        }

        const emp::block& get_block() const {
            return _block;
        }

        const emp::block& get_mac(const size_t pos) const {
            return _macs.at(pos);
        }

        // block ^= 1, macs unchanged
        void flip_block_lsb() {
            _block = _mm_xor_si128(_block, _mm_set_epi64x(0, 1));
        }
    };

    class ITMacBlockKeys {
        std::vector<emp::block> _localKeys;
        std::vector<emp::block> _globalKeys;
    public:
        ITMacBlockKeys(const std::vector<emp::block>& localKeys, std::vector<emp::block> globalKeys):
            _globalKeys {std::move(globalKeys)}
        {
            if (localKeys.size() != 128 * _globalKeys.size()) {
                throw std::invalid_argument{"Size of localKeys per global key is not 128."};
            }
            _localKeys.reserve(globalKeys.size());
            for (size_t i {0}; i != _globalKeys.size(); ++i) {
                _localKeys.push_back(polyval(localKeys.data() + i * 128));
            }
        }

        // `Fix` for a block
        ITMacBlockKeys(emp::NetIO&, BlockCorrelatedOT::Sender&);

        size_t global_key_size() const {
            return _globalKeys.size();
        }

        const emp::block& get_local_key(const size_t pos) const {
            return _localKeys.at(pos);
        }
        const emp::block& get_global_key(const size_t pos) const {
            return _globalKeys.at(pos);
        }

        // adding authenticated 1
        void flip_block_lsb() {
            for (size_t i {0}; i != global_key_size(); ++i) {
                _localKeys.at(i) = _mm_xor_si128(_localKeys.at(i), _globalKeys.at(i));
            }
        }
    };

    class ITMacBits {
        friend class ITMacBlocks;

        std::vector<bool> _bits;
        std::vector<emp::block> _macs; // _macs.size() == _bits.size() * deltaArrSize
    public:
        ITMacBits() = delete;

        /**
         * Direct construction.
         * @param bits Cannot be empty.
         * @param macs Must be a multiple of bits.size()
         */
        ITMacBits(std::vector<bool>&& bits, std::vector<emp::block>&& macs):
            _bits {std::move(bits)},
            _macs {std::move(macs)}
        {
            if (_macs.size() % _bits.size()) {
                throw std::runtime_error{"The size of macs is not a multiple of the size of bits."};
            }
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
        ITMacBits(emp::NetIO& io, BlockCorrelatedOT::Receiver& bCOTReceiver, std::vector<bool> bitsToFix):
            ITMacBits{bCOTReceiver, bitsToFix.size()}
        {

            // Compute XOR between the generated bits and the bits to fix, store the result in fixedBits
            // TODO: Compact the bool array to save communication
            auto* diffArr = new bool[_bits.size()];
            for (size_t i {0}; i < _bits.size(); ++i) {
                diffArr[i] = _bits.at(i) ^ bitsToFix.at(i);
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

        bool at(const size_t pos) const {
            return _bits.at(pos);
        }

        const emp::block& get_mac(const size_t bitPos, const size_t globalKeyPos) const {
            return _macs.at(bitPos + globalKeyPos * size());
        }

        ITMacBlocks polyval_to_Blocks() && {
            const size_t GLOBAL_KEY_SIZE {global_key_size()};
            return ITMacBlocks{_bits, std::move(_macs), GLOBAL_KEY_SIZE};
        }
    };

    class ITMacBitKeys {
        std::vector<emp::block> _localKeys;
        std::vector<emp::block> _globalKeys;

    public:
        ITMacBitKeys() = delete;
        ITMacBitKeys(std::vector<emp::block>&& localKeys, std::vector<emp::block> globalKeys):
            _localKeys {std::move(localKeys)},
            _globalKeys {std::move(globalKeys)}
        {
            if (_globalKeys.empty()) {
                throw std::runtime_error{"Global keys cannot be empty."};
            }
            if (_localKeys.size() % _globalKeys.size() != 0) {
                throw std::runtime_error{"The localKeys size if not a multiple of the globalKeys size."};
            }
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
                for (size_t iterBit {0}; iterBit < bitsSize; ++iterBit) {
                        _localKeys.at(iterBit + iterGlobalKey * bitsSize) ^=
                            _globalKeys.at(iterGlobalKey) & masks.at(iterBit);
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

        const emp::block& get_local_key(const size_t bitPos, const size_t globalKeyPos) const {
            return _localKeys.at(bitPos + globalKeyPos * size());
        }

        const emp::block& get_global_key(const size_t pos) const {
            return _globalKeys.at(pos);
        }

        ITMacBlockKeys polyval_to_Blocks() && {
            return ITMacBlockKeys{_localKeys, std::move(_globalKeys)};
        }
    };

}

#endif // ATLab_AUTHED_BIT_HPP

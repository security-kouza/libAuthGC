#ifndef ATLab_AUTHED_BIT_HPP
#define ATLab_AUTHED_BIT_HPP

#include <array>
#include <stdexcept>
#include <vector>
#include <boost/bind/bind.hpp>
#include <boost/core/span.hpp>

#include <emp-tool/utils/block.h>

#include "authed_bit.hpp"
#include "params.hpp"
#include "block_correlated_OT.hpp"
#include "ATLab/matrix.hpp"
#include "ATLab/traits.hpp"

namespace ATLab {
    class ITMacBlockKeys;
    class ITMacBlockSpan;
    class ITMacBlockKeySpan;

    class ITMacBlocks {
        friend class ITMacBlockSpan;
    public:
        using MacType = emp::block;

    private:
        std::vector<MacType> _macs; // flattened by global key then block index
        std::vector<emp::block> _blocks;
        const size_t _globalKeySize; // number of global keys

    public:
        ITMacBlocks(std::vector<emp::block>&& blocks, std::vector<emp::block>&& macs, size_t globalKeySize) noexcept;

        ITMacBlocks(
            boost::span<const emp::block> blocks,
            boost::span<const emp::block> macs,
            size_t globalKeySize
        ) noexcept;

        /**
         * @param bits size must be 128 * blockSize
         * @param macs size must be bits.size() * deltaArrSize
         */
        ITMacBlocks(const Bitset& bits, std::vector<emp::block> macs, size_t deltaArrSize);

        /**
         * Random ITMacBlock constructor
         * Protocol interactions involved.
         * @param blockSize Number of blocks to generate.
         */
        ITMacBlocks(const BlockCorrelatedOT::Receiver& bCOTReceiver, size_t blockSize);

        // `Fix` for blocks
        ITMacBlocks(ATLab::NetIO&, const BlockCorrelatedOT::Receiver&, std::vector<emp::block> blocksToAuth);

        // `Fix` for blocks
        ITMacBlocks(ATLab::NetIO&, const BlockCorrelatedOT::Receiver&, boost::span<const emp::block> blocksToAuth);

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

        /**
         * [[x]]_y => [[y]_x
         *
         * The authed block becomes the global key. The MAC becomes the local key.
         *
         * Only specifies one authed block with one global key, so the output is also of size 1x1
         */
        [[nodiscard]]
        ITMacBlockKeys swap_value_and_key(size_t globalKeyPos, size_t blockPos) const noexcept;

        /**
         * [[x]]_vec{y} => [[vec{y}]_x
         *
         * The authed block becomes the global key. The MACs becomes the local keys.
         */
        [[nodiscard]]
        ITMacBlockKeys swap_value_and_key() && noexcept;

        /**
         * [[x]]_y with MAC m => [[m]]_y^-1 with MAC x
         * By just swaping the blocks and the MACs
         */
        void inverse_value_and_mac() noexcept;

        friend ITMacBlocks operator*(const Matrix<bool>&, const ITMacBlocks&);
    };

    class ITMacScaledBits {
        Bitset _selectors;
        std::vector<emp::block> _macs;
        emp::block _scalar;

    public:
        ITMacScaledBits(
            ATLab::NetIO&,
            const BlockCorrelatedOT::Receiver&,
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
        friend class ITMacBlockKeySpan;

        std::vector<emp::block> _localKeys; // flattened by global key then block index
        std::vector<emp::block> _globalKeys;
    public:
        // Move localkeys only when globalKey with size 1
        ITMacBlockKeys(std::vector<emp::block>&& localKeys, const emp::block globalKey):
            _localKeys {std::move(localKeys)},
            _globalKeys {globalKey}
        {}

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
        ITMacBlockKeys(const BlockCorrelatedOT::Sender& bCOTSender, size_t blockSize);

        // `Fix` for blocks
        ITMacBlockKeys(ATLab::NetIO&, const BlockCorrelatedOT::Sender&, size_t blockSize);

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

        /**
         * [[x]]_y => [[y]_x
         *
         * The global key becomes the authed block. The local key becomes the MAC.
         *
         * Input/Output of size 1x1
         */
        [[nodiscard]]
        ITMacBlocks swap_value_and_key(size_t globalKeyPos, size_t blockPos) const noexcept;

        /**
         * [[x]]_vec{y} => [[vec{y}]_x
         *
         * The global keys become the authed blocks. The local keys become the MACs.
         *
         * Only support ITMacBlockKeys with only one block.
         */
        [[nodiscard]]
        ITMacBlocks swap_value_and_key() && noexcept;

        /**
         * [[x]]_y with MAC m => [[m]]_y^-1 with MAC x
         *
         * By setting global keys to their inverse, and local key *= inv(global key)
         */
        void inverse_value_and_mac() noexcept;

        friend ITMacBlockKeys operator*(const Matrix<bool>&, const ITMacBlockKeys&);
    };

    class ITMacBits {
        friend class ITMacBlocks;
        friend class ITMacScaledBits;

        using MacData = emp::block;
        Bitset _bits;
        std::vector<MacData> _macs; // _macs.size() == _bits.size() * deltaArrSize
    public:
        ITMacBits() = delete;
        ITMacBits(ITMacBits&&) = default;

        /**
         * Direct construction.
         * @param bits Cannot be empty.
         * @param macs Must be a multiple of bits.size()
         */
        ITMacBits(Bitset bits, std::vector<emp::block> macs):
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
        ITMacBits(const BlockCorrelatedOT::Receiver& bCOTReceiver, const size_t len):
            ITMacBits {[&bCOTReceiver, len]() -> ITMacBits {
                auto [u, m] {bCOTReceiver.extend(len)};
                return ITMacBits{std::move(u), std::move(m)};
            }()}
        {}

        /**
         * Fixed ITMacBit constructor, the `Fix` procedure defined in CYYW23.
         * Will invoke the random constructor first, and send to
         */
        ITMacBits(ATLab::NetIO& io, const BlockCorrelatedOT::Receiver& bCOTReceiver, Bitset bitsToFix):
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
            assert(_bits.size() != 0);
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

        /**
         * Open the bits to the other party who holds the corresponding ITMacBitKeys object.
         * Send first the bits, then the hash of all the MACs authed with key[0]
         */
        template <typename HashF>
        void open(ATLab::NetIO& io, HashF&& h, const size_t begin = 0, const size_t end = 0) const noexcept {
            assert(
                (end == 0 && begin == 0) ||
                (end != 0 && (
                    begin < size() && begin < end && end <= size()
                ))
            );

            send_boost_bitset(io, _bits, begin, end);

            const size_t openSize {(end == 0) ? size() : (end - begin)};
            const HashRes<HashF> hash {h(_macs.data() + begin, sizeof(MacData) * openSize)};
            io.send_data(hash.data(), sizeof(hash));
        }

        ITMacBlocks polyval_to_Blocks() && {
            const size_t GLOBAL_KEY_SIZE {global_key_size()};
            return ITMacBlocks{_bits, std::move(_macs), GLOBAL_KEY_SIZE};
        }

        ITMacBits extract_by_global_key(const size_t globalKeyIndex) const {
            const auto sliceBegin {_macs.cbegin() + globalKeyIndex * size()};
            const auto sliceEnd {sliceBegin + size()};
#ifdef DEBUG
            assert(sliceEnd <= _macs.cend());
#endif // DEBUG
            std::vector<emp::block> macSlice {sliceBegin, sliceEnd};

            return ITMacBits{_bits, std::move(macSlice)};
        }

        friend ITMacBits operator*(const Matrix<bool>&, const ITMacBits&);
    };

    class ITMacOpenedBits {
        friend class ITMacBitKeys;

        const Bitset _bits;
        ITMacOpenedBits() = delete;
        explicit ITMacOpenedBits(Bitset bits) noexcept: _bits {std::move(bits)} {}

    public:
        auto test(const size_t pos) const noexcept {
            return _bits.test(pos);
        }
    };

    class ITMacBitKeys {
        using LocalKey = emp::block;
        using GlobalKey = emp::block;

        std::vector<LocalKey> _localKeys;
        std::vector<GlobalKey> _globalKeys;
    public:
        ITMacBitKeys() = delete;
        ITMacBitKeys(ITMacBitKeys&&) = default;

        ITMacBitKeys(std::vector<emp::block> localKeys, std::vector<emp::block> globalKeys):
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
         * Construct keys for random authenticated bits.
         * Protocol interactions involved.
         * _bits will not be available.
         * @param len Number of bits to generate.
         */
        ITMacBitKeys(const BlockCorrelatedOT::Sender& bCOTSender, const size_t len):
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
        ITMacBitKeys(ATLab::NetIO& io, const BlockCorrelatedOT::Sender& bCOTSender, const size_t bitsSize):
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

        // size of bits authenticated
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

        /**
         * Open to get the bits from the other party.
         */
        template <typename HashF>
        [[nodiscard]]
        ITMacOpenedBits open(ATLab::NetIO& io, HashF&& h, const size_t begin = 0, size_t end = 0) const {
            assert(
                (end == 0 && begin == 0) ||
                (end != 0 && (
                    begin < size() && begin < end && end <= size()
                ))
            );

            if (end == 0) {
                end = size();
            }
            const size_t sliceSize {end - begin};

            using HashRes = HashRes<HashF>;
            static_assert(std::is_same_v<LocalKey, GlobalKey>); // For xoring
            /**
             * First, receive the bits.
             * Next, receive the hash of all the MACs authed with key[0].
             * Finally, compare with the hash of the local keys. Abort if the two hashes are not equal.
             */

            std::vector<LocalKey> bufKeys;
            bufKeys.reserve(size());

            Bitset bits {receive_boost_bitset(io, sliceSize)};
            const auto& globalKey {get_global_key(0)};
            for (size_t i {0}; i < sliceSize; ++i) {
                bufKeys.push_back(_mm_xor_si128(
                    get_local_key(0, begin + i),
                    and_all_bits(bits.test(i), globalKey)
                ));
            }
            HashRes localHash {h(bufKeys.data(), sizeof(LocalKey) * bufKeys.size())};

            HashRes macHash {};
            io.recv_data(macHash.data(), sizeof(macHash));
            if (localHash != macHash) {
                throw std::runtime_error{"The hashes of local keys and MACs are not equal."};
            }

            return ITMacOpenedBits{std::move(bits)};
        }

        ITMacBlockKeys polyval_to_Blocks() && {
            return ITMacBlockKeys{_localKeys, std::move(_globalKeys)};
        }

        ITMacBitKeys extract_by_global_key(const size_t globalKeyIndex) const {
            const auto sliceBegin {_localKeys.cbegin() + globalKeyIndex * size()};
            const auto sliceEnd {sliceBegin + size()};
#ifdef DEBUG
            assert(sliceEnd <= _localKeys.cend());
#endif // DEBUG
            std::vector<emp::block> localKeySlice {sliceBegin, sliceEnd};

            return ITMacBitKeys{std::move(localKeySlice), {_globalKeys.at(globalKeyIndex)}};
        }

        friend ITMacBitKeys operator*(const Matrix<bool>&, const ITMacBitKeys&);
    };

    [[nodiscard]]
    bool check_same_bit(ATLab::NetIO& io, const ITMacBlockKeySpan& key0, const ITMacBlockKeySpan& key1) noexcept;

    [[nodiscard]]
    bool check_same_bit(ATLab::NetIO& io, const ITMacBlockSpan& block0, const ITMacBlockSpan& block1) noexcept;

    // A span with only one globalKey
    class ITMacBlockSpan {
        const ITMacBlocks& _authedBlocks;
        const size_t _globalKeyPos;
        const size_t _begin;
        const size_t _end;

        constexpr static size_t MAX_SIZE_ {std::numeric_limits<size_t>::max()};
    public:
        // Empty span when begin == end
        ITMacBlockSpan(
            const ITMacBlocks& authedBlocks,
            const size_t globalKeyPos,
            const size_t begin,
            const size_t end
        ):
            _authedBlocks {authedBlocks},
            _globalKeyPos {globalKeyPos},
            _begin {begin},
            _end {end}
        {
            assert(authedBlocks.size() != 0);
            assert(globalKeyPos < authedBlocks.global_key_size());
            assert(begin <= end);
            assert(end <= authedBlocks.size());
        }

        ITMacBlockSpan(const ITMacBlocks& block): ITMacBlockSpan {block, 0, 0, block.size()} {}

        [[nodiscard]]
        size_t size() const noexcept {
            return _end - _begin;
        }

        [[nodiscard]]
        const auto& get_block(const size_t i) const noexcept {
            assert(_begin + i < _end);
            assert(i <= MAX_SIZE_ - _begin);

            return _authedBlocks.get_block(_begin + i);
        }

        [[nodiscard]]
        const auto& get_mac(const size_t i) const noexcept {
            assert(_begin + i < _end);
            assert(i <= MAX_SIZE_ - _begin);

            return _authedBlocks.get_mac(_globalKeyPos, _begin + i);
        }

        [[nodiscard]]
        boost::span<const ITMacBlocks::MacType> mac_span() const noexcept {
            return {_authedBlocks._macs.data() + _authedBlocks.size() * _globalKeyPos + _begin, _end - _begin};
        }
    };

    class ITMacBlockKeySpan {
        const ITMacBlockKeys& _authedBlockKeys;
        const size_t _globalKeyPos;
        const size_t _begin;
        const size_t _end;

        constexpr static size_t MAX_SIZE_ {std::numeric_limits<size_t>::max()};
    public:
        ITMacBlockKeySpan(
            const ITMacBlockKeys& authedBlockKeys,
            const size_t globalKeyPos,
            const size_t begin,
            const size_t end
        ):
            _authedBlockKeys {authedBlockKeys},
            _globalKeyPos {globalKeyPos},
            _begin {begin},
            _end {end}
        {
            assert(authedBlockKeys.size() != 0);
            assert(globalKeyPos < authedBlockKeys.global_key_size());
            assert(begin <= end);
            assert(end <= authedBlockKeys.size());
        }

        ITMacBlockKeySpan(const ITMacBlockKeys& key): ITMacBlockKeySpan{key, 0, 0, key.size()} {}

        [[nodiscard]]
        size_t size() const noexcept {
            return _end - _begin;
        }

        [[nodiscard]]
        const auto& get_local_key(const size_t i) const noexcept {
            assert(_begin + i < _end);
            assert(i <= MAX_SIZE_ - _begin);

            return _authedBlockKeys.get_local_key(_globalKeyPos, _begin + i);
        }

        [[nodiscard]]
        const auto& global_key() const noexcept {
            return _authedBlockKeys.get_global_key(_globalKeyPos);
        }

        [[nodiscard]]
        boost::span<const emp::block> local_key_span() const noexcept {
            return {
                _authedBlockKeys._localKeys.data() + _authedBlockKeys.size() * _globalKeyPos + _begin,
                _end - _begin
            };
        }
    };

    // Prove the authed blocks by different global keys are the same
    void eqcheck_diff_key(
        ATLab::NetIO& io,
        const ITMacBlockSpan& authedBlocks0,
        const ITMacBlockSpan& authedBlocks1
    ) noexcept;

    // Check the authed blocks by different global keys are the same.
    // Throw if check fails
    void eqcheck_diff_key(
        ATLab::NetIO& io,
        const ITMacBlockKeySpan& authedBlocks0,
        const ITMacBlockKeySpan& authedBlocks1
    );
}

#endif // ATLab_AUTHED_BIT_HPP

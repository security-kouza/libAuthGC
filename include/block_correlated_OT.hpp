#ifndef ATLab_CORRELATED_OT_HPP
#define ATLab_CORRELATED_OT_HPP

#include <utility>
#include <vector>
#include <memory>
#include <stdexcept>
#include <array>
#include <emp-tool/utils/block.h>
#include <emp-ot-original/iknp.h>

#include "EndemicOT/EndemicOT.hpp"
#include "utils.hpp"
#include "PRNG.hpp"

#ifndef PARTY_INSTANCES_PER_THREAD
#define PARTY_INSTANCES_PER_THREAD 1 // Always keep this at 1 outside single-process testing.
#endif
static_assert(PARTY_INSTANCES_PER_THREAD == 1 || PARTY_INSTANCES_PER_THREAD == 2,
    "PARTY_INSTANCES_PER_THREAD must be 1 (production) or 2 (single-process testing only)");

// using IKNP
namespace ATLab::BlockCorrelatedOT {

    using OT = emp::IKNP<emp::NetIO>;

    class Sender {
#if PARTY_INSTANCES_PER_THREAD == 1
        static std::unique_ptr<OT>& shared_ot_storage() {
            static std::unique_ptr<OT> instance;
            return instance;
        }
#else
        static std::array<std::unique_ptr<OT>, PARTY_INSTANCES_PER_THREAD>& shared_ot_storage() {
            static std::array<std::unique_ptr<OT>, PARTY_INSTANCES_PER_THREAD> instances;
            return instances;
        }

        static size_t role_index(const emp::NetIO::Role role) {
            const auto index = static_cast<size_t>(role);
            if (index >= PARTY_INSTANCES_PER_THREAD) {
                throw std::logic_error{"Unexpected NetIO role index for BlockCorrelatedOT sender"};
            }
            return index;
        }
#endif

    public:
        static OT& Initialize_simple_OT(emp::NetIO& io) {
#if PARTY_INSTANCES_PER_THREAD == 1
            auto& instance = shared_ot_storage();
#else
            auto& instance = shared_ot_storage().at(role_index(io.role));
#endif
            if (!instance) {
                instance = std::make_unique<OT>(&io, true);
                instance->setup_send();
            } else if (instance->io != &io) {
                throw std::runtime_error{"Shared IKNP sender OT already bound to a different NetIO"};
            }
            return *instance;
        }

        static OT& Get_simple_OT(const emp::NetIO::Role role) {
#if PARTY_INSTANCES_PER_THREAD == 1
            auto& instance = shared_ot_storage();
#else
            auto& instance = shared_ot_storage().at(role_index(role));
#endif
            if (!instance) {
                throw std::logic_error{"Shared IKNP sender OT is not initialized"};
            }
            return *instance;
        }

        const std::vector<emp::block> _deltaArr;
        const emp::NetIO::Role role;
        const size_t deltaArrSize;
        Sender(emp::NetIO& io, std::vector<emp::block> deltaArr) :
            _deltaArr(std::move(deltaArr)),
            role {io.role},
            deltaArrSize {_deltaArr.size()}
        {
            Initialize_simple_OT(io);
        }

        /**
         *
         * @param len the returned OT length of each delta
         * @return The size is `len * _deltaArrSize`. Arrange: key major
         * The j-th key corresponding to the i-th delta is placed at the position `j + i * len` (counting from 0).
         */
        std::vector<emp::block> extend(const size_t len) const {
            const size_t otSize {len * _deltaArr.size()};
            std::vector<emp::block> k1Vec(otSize), k2Vec(otSize);
            for (auto& k1 : k1Vec) {
                k1 = _mm_set_epi64x(THE_GLOBAL_PRNG(), THE_GLOBAL_PRNG());
            }
            for (size_t i {0}; i != _deltaArr.size(); ++i) {
                for (size_t j {0}; j != len; ++j) {
                    k2Vec.at(i * len + j) = k1Vec.at(i * len + j) ^ _deltaArr.at(i);
                }
            }
            Get_simple_OT(role).send(k1Vec.data(), k2Vec.data(), static_cast<int64_t>(otSize));
            return k1Vec;
        }

        const emp::block& get_delta(const size_t i) const {
            return _deltaArr.at(i);
        }

        const std::vector<emp::block>& get_delta_arr() const {
            return _deltaArr;
        }
    };

    class Receiver {
#if PARTY_INSTANCES_PER_THREAD == 1
        static std::unique_ptr<OT>& shared_ot_storage() {
            static std::unique_ptr<OT> instance;
            return instance;
        }
#else
        static std::array<std::unique_ptr<OT>, PARTY_INSTANCES_PER_THREAD>& shared_ot_storage() {
            static std::array<std::unique_ptr<OT>, PARTY_INSTANCES_PER_THREAD> instances;
            return instances;
        }

        static size_t role_index(const emp::NetIO::Role role) {
            const auto index = static_cast<size_t>(role);
            if (index >= PARTY_INSTANCES_PER_THREAD) {
                throw std::logic_error{"Unexpected NetIO role index for BlockCorrelatedOT receiver"};
            }
            return index;
        }
#endif

    public:
        static OT& Initialize_simple_OT(emp::NetIO& io) {
#if PARTY_INSTANCES_PER_THREAD == 1
            auto& instance = shared_ot_storage();
#else
            auto& instance = shared_ot_storage().at(role_index(io.role));
#endif
            if (!instance) {
                instance = std::make_unique<OT>(&io, true);
                instance->setup_recv();
            } else if (instance->io != &io) {
                throw std::runtime_error{"Shared IKNP receiver OT already bound to a different NetIO"};
            }
            return *instance;
        }

        static OT& Get_simple_OT(const emp::NetIO::Role role) {
#if PARTY_INSTANCES_PER_THREAD == 1
            auto& instance = shared_ot_storage();
#else
            const auto& instance = shared_ot_storage().at(role_index(role));
#endif
            if (!instance) {
                throw std::logic_error{"Shared IKNP receiver OT is not initialized"};
            }
            return *instance;
        }

        const size_t deltaArrSize; // L
        const emp::NetIO::Role role;

        Receiver(emp::NetIO& io, const size_t deltaArrSize) :
            deltaArrSize {deltaArrSize},
            role {io.role}
        {
            Initialize_simple_OT(io);
        }

        std::tuple<Bitset, std::vector<emp::block>> extend(const size_t len) const {
            const size_t otSize{len * deltaArrSize};
            Bitset choices{random_dynamic_bitset(len)};
            std::vector<emp::block> macArr(otSize);

            // Use regular bool array instead of vector<bool>
            bool* choicesForOT = new bool[otSize];
            // repeatedly copy choices to choicesForOT
            for (size_t i{0}; i != deltaArrSize; ++i) {
                for (size_t j = 0; j < len; ++j) {
                    choicesForOT[i * len + j] = choices[j];
                }
            }

            Get_simple_OT(role).recv(macArr.data(), choicesForOT, static_cast<int64_t>(otSize));

            delete[] choicesForOT;
            return {choices, macArr};
        }
    };
}

#endif //ATLab_CORRELATED_OT_HPP

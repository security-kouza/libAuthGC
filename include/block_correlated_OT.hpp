#ifndef ATLab_CORRELATED_OT_HPP
#define ATLab_CORRELATED_OT_HPP

#include <utility>
#include <vector>
#include <memory>
#include <stdexcept>
#include <emp-tool/utils/block.h>
#include <emp-ot-original/iknp.h>

#include "EndemicOT/EndemicOT.hpp"
#include "utils.hpp"
#include "PRNG.hpp"

// using IKNP
namespace ATLab::BlockCorrelatedOT {

    using OT = emp::IKNP<emp::NetIO>;

    class Sender {
        static std::unique_ptr<OT>& shared_ot_storage() {
            static std::unique_ptr<OT> instance;
            return instance;
        }

        static OT& get_simple_ot(emp::NetIO& io) {
            auto& instance = shared_ot_storage();
            if (!instance) {
                instance = std::make_unique<OT>(&io, false);
                instance->setup_send();
            } else if (instance->io != &io) {
                throw std::runtime_error{"Shared IKNP sender OT already bound to a different NetIO"};
            }
            return *instance;
        }

        static OT& get_simple_ot() {
            auto& instance = shared_ot_storage();
            if (!instance) {
                throw std::logic_error("Shared IKNP sender OT is not initialized");
            }
            return *instance;
        }

        const std::vector<emp::block> _deltaArr;
    public:
        const size_t deltaArrSize;
        Sender(emp::NetIO& io, std::vector<emp::block> deltaArr) :
            _deltaArr(std::move(deltaArr)),
            deltaArrSize {_deltaArr.size()}
        {
            get_simple_ot(io);
        }

        /**
         *
         * @param len the returned OT length of each delta
         * @return The size is `len * _deltaArrSize`. Arrange: keys of the first delta, keys of the second ...
         * The j-th key corresponding to the i-th delta is placed at the position `j + i * len` (counting from 0).
         */
        std::vector<emp::block> extend(const size_t len) {
            const size_t otSize {len * _deltaArr.size()};
            std::vector<emp::block> k1Vec(otSize), k2Vec(otSize);
            auto prg = THE_GLOBAL_PRNG;
            for (auto& k1 : k1Vec) {
                k1 = as_block(prg());
            }
            for (size_t i {0}; i != _deltaArr.size(); ++i) {
                for (size_t j {0}; j != len; ++j) {
                    k2Vec.at(i * len + j) = k1Vec.at(i * len + j) ^ _deltaArr.at(i);
                }
            }
            get_simple_ot().send(k1Vec.data(), k2Vec.data(), otSize);
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
        static std::unique_ptr<OT>& shared_ot_storage() {
            static std::unique_ptr<OT> instance;
            return instance;
        }

        static OT& get_simple_ot(emp::NetIO& io) {
            auto& instance = shared_ot_storage();
            if (!instance) {
                instance = std::make_unique<OT>(&io, false);
                instance->setup_recv();
            } else if (instance->io != &io) {
                throw std::runtime_error{"Shared IKNP receiver OT already bound to a different NetIO"};
            }
            return *instance;
        }

        static OT& get_simple_ot() {
            auto& instance = shared_ot_storage();
            if (!instance) {
                throw std::logic_error("Shared IKNP receiver OT is not initialized");
            }
            return *instance;
        }

    public:
        const size_t deltaArrSize; // L

        Receiver(emp::NetIO& io, const size_t deltaArrSize) :
            deltaArrSize {deltaArrSize}
        {
            get_simple_ot(io);
        }

        std::tuple<Bitset, std::vector<emp::block>> extend(const size_t len) {
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

            get_simple_ot().recv(macArr.data(), choicesForOT, otSize);

            delete[] choicesForOT;
            return {choices, macArr};
        }
    };
}

#endif //ATLab_CORRELATED_OT_HPP

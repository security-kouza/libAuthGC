#ifndef ATLab_CORRELATED_OT_HPP
#define ATLab_CORRELATED_OT_HPP

#include <vector>
#include <emp-tool/utils/block.h>
#include <emp-ot-original/iknp.h>

#include "EndemicOT/EndemicOT.hpp"
#include "utils.hpp"
#include "PRNG.hpp"

// using IKNP
namespace ATLab::BlockCorrelatedOT {

    // TODO: possible optimization: making OT shared across IKNP instances

    using OT = emp::IKNP<emp::NetIO>;

    class Sender {
        const std::vector<emp::block> _deltaArr;
        OT _ot;
    public:
        const size_t deltaArrSize;
        Sender(emp::NetIO& io, const std::vector<emp::block>& deltaArr) :
            _deltaArr(deltaArr),
            _ot {&io, false},
            deltaArrSize {_deltaArr.size()}
        {
            _ot.setup_send();
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
            auto prg = PRNG_Kyber::get_PRNG_Kyber();
            for (auto& k1 : k1Vec) {
                k1 = as_block(prg());
            }
            for (size_t i {0}; i != _deltaArr.size(); ++i) {
                for (size_t j {0}; j != len; ++j) {
                    k2Vec.at(i * len + j) = k1Vec.at(i * len + j) ^ _deltaArr.at(i);
                }
            }
            _ot.send(k1Vec.data(), k2Vec.data(), otSize);
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
        OT _ot;
    public:
        const size_t deltaArrSize; // L

        Receiver(emp::NetIO& io, const size_t deltaArrSize) :
            _ot {&io, false},
            deltaArrSize {deltaArrSize}
        {
            _ot.setup_recv();
        }

        std::tuple<Bitset, std::vector<emp::block>> extend(const size_t len) {
            const size_t otSize{len * deltaArrSize};
            Bitset choices{random_bool_vector(len)};
            std::vector<emp::block> macArr(otSize);

            // Use regular bool array instead of vector<bool>
            bool* choicesForOT = new bool[otSize];
            // repeatedly copy choices to choicesForOT
            for (size_t i{0}; i != deltaArrSize; ++i) {
                for (size_t j = 0; j < len; ++j) {
                    choicesForOT[i * len + j] = choices[j];
                }
            }

            _ot.recv(macArr.data(), choicesForOT, otSize);

            delete[] choicesForOT;
            return {choices, macArr};
        }
    };
}

#endif //ATLab_CORRELATED_OT_HPP

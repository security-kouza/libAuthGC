#ifndef ATLab_CORRELATED_OT_HPP
#define ATLab_CORRELATED_OT_HPP

#include <vector>
#include <emp-tool/utils/block.h>
#include <emp-ot-original/iknp.h>

#include "EndemicOT/EndemicOT.hpp"

// using IKNP
namespace ATLab::BlockCorrelatedOT {

    // TODO: possible optimization: making OT shared across IKNP instances

    using OT = emp::IKNP<emp::NetIO>;

    class Sender {
        const std::vector<emp::block> _deltaArr;
        const size_t _deltaArrSize;
        OT _ot;
    public:
        Sender(emp::NetIO& io, const std::vector<emp::block>& deltaArr) :
            _deltaArr(deltaArr),
            _deltaArrSize {_deltaArr.size()},
            _ot {&io, false}
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
            auto prg = ATLab::PRNG_Kyber::get_PRNG_Kyber();
            for (auto& k1 : k1Vec) {
                k1 = ATLab::as_block(prg());
            }
            for (size_t i {0}; i != _deltaArr.size(); ++i) {
                for (size_t j {0}; j != len; ++j) {
                    k2Vec.at(i * len + j) = k1Vec.at(i * len + j) ^ _deltaArr.at(i);
                }
            }
            _ot.send(k1Vec.data(), k2Vec.data(), otSize);
            return k1Vec;
        }
    };

    class Receiver {
        const size_t _deltaArrSize; // L
        OT _ot;
    public:
        Receiver(emp::NetIO& io, const size_t deltaArrSize) :
            _deltaArrSize {deltaArrSize},
            _ot {&io, false}
        {
            _ot.setup_recv();
        }

        std::tuple<std::vector<bool>, std::vector<emp::block>> extend(const size_t len) {
            const size_t otSize{len * _deltaArrSize};
            std::vector<bool> choices{random_bool_vector(len)};
            std::vector<emp::block> macArr(otSize);

            // Use regular bool array instead of vector<bool>
            bool* choicesForOT = new bool[otSize];
            // repeatedly copy choices to choicesForOT
            for (size_t i{0}; i != _deltaArrSize; ++i) {
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

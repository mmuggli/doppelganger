#ifndef CHARVECTOR_H
#define CHARVECTOR_H
#include <fstream>
#include "../misc/definitions.h"
#include "rlevector.h"
#include "bitvector.h"
#include "nibblevector.h"
#include <map>
#include <set>
#include "deltavector.h"
#include <sdsl/suffix_arrays.hpp>

namespace CSA
{

class CharVector
{
public:
    void populate(usint c, DeltaVector::Encoder*, usint offset);
    void populate(usint c, DeltaVector *);
    void syncFMIndex();
    void writeTo(std::ofstream& file) const;
    void load(std::ifstream& file);
    void writeTo(FILE* file) const;
    usint reportSize() const;
//    usint rank(usint c, usint num, bool at_least = false) const ;
    std::vector<usint> restricted_unique_range_values(usint l, usint r, usint min, usint max) const;//FIXME: don't copy vector in return
    void array_restricted_unique_range_values(usint l, usint r, usint min, usint max, std::set<usint>& ) const;
    usint maxlength() const;
    void constructF(   sdsl::int_vector<1u> &a_inedgetest);
    void setwt(sdsl::int_vector<> &v);
    unsigned outgoing_rank(usint i) const {return outgoing_ranks[i];}
    void constructM(sdsl::int_vector<> &a_outgoing_ranks) {outgoing_ranks = a_outgoing_ranks;}
    inline std::map<usint, CSA::BitVector*>::const_iterator begin() const { return array.begin();}
    inline std::map<usint, CSA::BitVector*>::const_iterator end() const { return array.end();}
    class Iterator {
    public:
        Iterator(BitVector::Iterator *itr);
        ~Iterator();
        usint select(usint);
        usint selectNext();
        usint rank(usint, bool at_least = false);
        bool isSet(usint);
  

    private:
        BitVector::Iterator *itr;
    };

    Iterator *newIterator(usint c, char *, char *) const;

    size_t iterSize(usint c) const;
private:
    sdsl::wt_int<> *wt;
    sdsl::int_vector<> wt_data;
    sdsl::int_vector<> outgoing_ranks;
    std::map<usint, CSA::BitVector*> array;
    // sdsl::csa_wt<sdsl::wt_int<>, 
    //              64, 
    //              64, 
    //              sdsl::sa_order_sa_sampling<>, 
    //              sdsl::int_vector<>, 
    //              sdsl::int_alphabet<>
    //              > fm_index;


    sdsl::int_vector<1u> inedgetest;
    sdsl::bit_vector::select_1_type *b_sel;
};

} // namespace CSA


#endif // CHARVECTOR_H

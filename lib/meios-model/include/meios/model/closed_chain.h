#ifndef HPP_GUARD_MEIOS_MODEL_MODEL_CLOSED_CHAIN_H
#define HPP_GUARD_MEIOS_MODEL_MODEL_CLOSED_CHAIN_H

#include "meios/model/tree.h"

#include "meios/records/loop_constraint.h"

#include "meios/math/rotations.h"

#include <vector>

namespace meios
{

template <typename Scalar = double, typename Rot = rpy<Scalar>>
struct closed_chain
{
    tree<Scalar, Rot> base;
    std::vector<loop_constraint<Scalar>> loops;
};

}

#endif

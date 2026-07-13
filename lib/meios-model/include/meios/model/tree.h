#ifndef HPP_GUARD_MEIOS_MODEL_MODEL_TREE_H
#define HPP_GUARD_MEIOS_MODEL_MODEL_TREE_H

#include "meios/records/link.h"
#include "meios/records/joint.h"

#include "meios/math/rotations.h"

#include <string>
#include <vector>

namespace meios
{

template <typename Scalar = double, typename Rot = rpy<Scalar>>
struct tree
{
    std::string name;
    std::vector<link<Scalar>> links;
    std::vector<joint<Scalar>> joints;
    std::vector<int> parent_of;
};

}

#endif

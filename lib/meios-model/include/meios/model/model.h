#ifndef HPP_GUARD_MEIOS_MODEL_MODEL_MODEL_H
#define HPP_GUARD_MEIOS_MODEL_MODEL_MODEL_H

#include "meios/model/structure.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/loop_constraint.h"

#include "meios/math/rotations.h"

#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

namespace meios
{

template <typename Scalar = double>
struct model
{
    std::string name;
    structure kind;
    std::variant<rpy<Scalar>, quaternion<Scalar>, axis_angle<Scalar>> rotation;
    std::vector<link<Scalar>> links;
    std::vector<joint<Scalar>> joints;
    std::vector<loop_constraint<Scalar>> loops;
    std::unordered_map<std::string, int> link_index;
    std::unordered_map<std::string, int> joint_index;
};

}

#endif

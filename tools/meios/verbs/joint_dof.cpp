#include "joint_dof.h"

#include "meios/model/model.h"
#include "meios/records/joint.h"

#include <cstddef>
#include <algorithm>

namespace meios::cli
{

std::size_t motion_dimension(joint_kind kind)
{
    switch(kind)
    {
        case joint_kind::fixed:      return 0;
        case joint_kind::revolute:   return 1;
        case joint_kind::continuous: return 1;
        case joint_kind::prismatic:  return 1;
        case joint_kind::planar:     return 3;
        case joint_kind::floating:   return 6;
    }
    return 0;
}

std::size_t total_dof(const model<double> &robot)
{
    std::size_t total = 0;
    for(const joint<double> &edge : robot.joints)
        total += motion_dimension(edge.kind);
    return total;
}

std::size_t independent_dof(const model<double> &robot)
{
    std::size_t constrained = 0;
    for(const joint<double> &edge : robot.joints)
        if(edge.couple)
            constrained += std::min<std::size_t>(1, motion_dimension(edge.kind));
    return total_dof(robot) - constrained;
}

}

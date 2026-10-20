#ifndef HPP_GUARD_MEIOS_CLI_JOINT_DOF_H
#define HPP_GUARD_MEIOS_CLI_JOINT_DOF_H

#include "meios/model/model.h"
#include "meios/records/joint.h"

#include <cstddef>

namespace meios::cli
{

// The dimension of the group the joint moves in: fixed is trivial, revolute and continuous move
// in SO(2), prismatic in R, planar in SE(2), floating in SE(3).
std::size_t motion_dimension(joint_kind kind);

std::size_t total_dof(const model<double> &robot);

// A mimic is the holonomic constraint q_m = k*q + c, so it removes one coordinate from a joint
// whose group has any and none from a fixed joint.
std::size_t independent_dof(const model<double> &robot);

}

#endif

#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_JOINT_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_JOINT_H

#include "meios/records/extension.h"
#include "meios/records/joint_extras.h"

#include "meios/math/vector3.h"
#include "meios/math/transform.h"

#include <string>
#include <vector>
#include <optional>

namespace meios
{

enum class joint_kind
{
    fixed,
    revolute,
    continuous,
    prismatic,
    floating,
    planar
};

template <typename Scalar = double>
struct joint
{
    std::string name;
    joint_kind kind;
    std::string parent;
    std::string child;
    transform<Scalar> origin;
    vector3<Scalar> axis;
    std::optional<joint_limits<Scalar>> limits;
    std::optional<mimic> couple;
    std::optional<dynamics<Scalar>> dyn;
    std::optional<safety_controller<Scalar>> safety;
    std::optional<calibration<Scalar>> calib;
    bool closes_loop;
    std::vector<extension> extensions;
};

}

#endif

#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_JOINT_EXTRAS_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_JOINT_EXTRAS_H

#include <string>
#include <optional>

namespace meios
{

template <typename Scalar = double>
struct joint_limits
{
    Scalar lower;
    Scalar upper;
    Scalar effort;
    Scalar velocity;
};

struct mimic
{
    std::string joint;
    double multiplier;
    double offset;
};

template <typename Scalar = double>
struct dynamics
{
    Scalar damping;
    Scalar friction;
};

template <typename Scalar = double>
struct safety_controller
{
    Scalar soft_lower_limit;
    Scalar soft_upper_limit;
    Scalar k_position;
    Scalar k_velocity;
};

template <typename Scalar = double>
struct calibration
{
    std::optional<Scalar> rising;
    std::optional<Scalar> falling;
};

}

#endif

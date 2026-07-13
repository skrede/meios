#ifndef HPP_GUARD_MEIOS_MODEL_MATH_ROTATIONS_H
#define HPP_GUARD_MEIOS_MODEL_MATH_ROTATIONS_H

#include "meios/math/vector3.h"

namespace meios
{

template <typename Scalar = double>
struct rpy
{
    Scalar roll;
    Scalar pitch;
    Scalar yaw;
};

template <typename Scalar = double>
struct quaternion
{
    Scalar w;
    Scalar x;
    Scalar y;
    Scalar z;
};

template <typename Scalar = double>
struct axis_angle
{
    vector3<Scalar> axis;
    Scalar angle;
};

}

#endif

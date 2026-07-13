#ifndef HPP_GUARD_MEIOS_MODEL_MATH_TRANSFORM_H
#define HPP_GUARD_MEIOS_MODEL_MATH_TRANSFORM_H

#include "meios/math/vector3.h"
#include "meios/math/rotations.h"

namespace meios
{

template <typename Scalar = double, typename Rot = rpy<Scalar>>
struct transform
{
    vector3<Scalar> translation;
    Rot rotation;
};

}

#endif

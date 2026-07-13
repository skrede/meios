#ifndef HPP_GUARD_MEIOS_MODEL_MATH_INERTIA_H
#define HPP_GUARD_MEIOS_MODEL_MATH_INERTIA_H

namespace meios
{

template <typename Scalar = double>
struct inertia
{
    Scalar ixx;
    Scalar ixy;
    Scalar ixz;
    Scalar iyy;
    Scalar iyz;
    Scalar izz;
};

}

#endif

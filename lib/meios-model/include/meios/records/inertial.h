#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_INERTIAL_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_INERTIAL_H

#include "meios/math/inertia.h"
#include "meios/math/transform.h"

namespace meios
{

template <typename Scalar = double>
struct inertial
{
    transform<Scalar> origin;
    Scalar mass;
    inertia<Scalar> tensor;
};

}

#endif

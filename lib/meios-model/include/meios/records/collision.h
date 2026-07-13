#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_COLLISION_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_COLLISION_H

#include "meios/records/geometry.h"

#include "meios/math/transform.h"

namespace meios
{

template <typename Scalar = double>
struct collision
{
    transform<Scalar> origin;
    geometry<Scalar> geom;
};

}

#endif

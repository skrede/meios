#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_VISUAL_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_VISUAL_H

#include "meios/records/geometry.h"
#include "meios/records/material.h"

#include "meios/math/transform.h"

#include <string>
#include <optional>

namespace meios
{

template <typename Scalar = double>
struct visual
{
    transform<Scalar> origin;
    geometry<Scalar> geom;
    std::optional<std::string> material_ref;
    std::optional<material<Scalar>> material_inline;
};

}

#endif

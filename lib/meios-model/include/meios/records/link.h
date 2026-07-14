#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_LINK_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_LINK_H

#include "meios/records/visual.h"
#include "meios/records/collision.h"
#include "meios/records/extension.h"
#include "meios/records/inertial.h"

#include "meios/diagnostic/source_location.h"

#include <string>
#include <vector>
#include <optional>

namespace meios
{

template <typename Scalar = double>
struct link
{
    std::string name;
    std::optional<inertial<Scalar>> body;
    std::vector<visual<Scalar>> visuals;
    std::vector<collision<Scalar>> collisions;
    std::vector<extension> extensions;
    std::optional<source_location> origin_loc;
};

}

#endif

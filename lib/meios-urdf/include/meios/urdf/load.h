#ifndef HPP_GUARD_MEIOS_URDF_LOAD_H
#define HPP_GUARD_MEIOS_URDF_LOAD_H

#include "meios/urdf/policy.h"

#include "meios/diagnostic/missing_asset.h"
#include "meios/diagnostic/topology_policy.h"

#include "meios/model/model.h"

#include <filesystem>

namespace meios
{

struct load_options
{
    load_options()
        : on_missing(missing_asset::warn),
          topology(topology_policy::fail),
          materials(material_policy::warn),
          strict(strictness::strict)
    {
    }

    missing_asset   on_missing;
    topology_policy topology;
    material_policy materials;
    strictness      strict;
};

model<double> load(const std::filesystem::path &path, const load_options &opts);

}

#endif

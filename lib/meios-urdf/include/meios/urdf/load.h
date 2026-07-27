#ifndef HPP_GUARD_MEIOS_URDF_LOAD_H
#define HPP_GUARD_MEIOS_URDF_LOAD_H

#include "meios/urdf/policy.h"

#include "meios/diagnostic/load_error.h"
#include "meios/diagnostic/missing_asset.h"
#include "meios/diagnostic/topology_policy.h"

#include "meios/model/model.h"

#include "meios/expected.h"

#include "meios/xacro/eval_policy.h"

#include "meios/diagnostic/log_sink.h"

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

namespace meios
{

class source_stack;
class evaluator_handle;

struct load_options
{
    load_options()
        : on_missing(missing_asset::warn),
          topology(topology_policy::fail),
          materials(material_policy::warn),
          strict(strictness::fail),
          eval(eval_policy::fail),
          backend(),
          package_roots(),
          args()
    {
    }

    missing_asset   on_missing;
    topology_policy topology;
    material_policy materials;
    strictness      strict;
    eval_policy     eval;

    std::shared_ptr<evaluator_handle> backend;

    std::vector<std::filesystem::path> package_roots;

    // Caller overrides seeded into the evaluation scope before expansion; because
    // the declared-default seed is seed-if-absent, a name set here wins over the
    // document's <xacro:arg> default.
    std::map<std::string, std::string> args;
};

expected<model<double>, load_error> load(const std::filesystem::path &path,
                                         const load_options &opts, log_sink &log);

expected<model<double>, load_error> load(const std::filesystem::path &path,
                                         const load_options &opts, source_stack &sources,
                                         log_sink &log);

expected<model<double>, load_error> load(const std::filesystem::path &path,
                                         const load_options &opts = {});

}

#endif

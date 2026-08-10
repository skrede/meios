#ifndef HPP_GUARD_MEIOS_URDF_LOAD_H
#define HPP_GUARD_MEIOS_URDF_LOAD_H

#include "meios/urdf/policy.h"
#include "meios/urdf/load_result.h"

#include "meios/diagnostic/load_error.h"
#include "meios/diagnostic/missing_asset.h"
#include "meios/diagnostic/topology_policy.h"

#include "meios/model/model.h"

#include "meios/config.h"
#include "meios/expected.h"

#include "meios/xacro/eval_policy.h"

#ifdef MEIOS_HAS_YAML
    #include "meios/yaml/parser.h"
#endif

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/capturing_log_sink.h"

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

namespace meios
{

class source_stack;
class evaluator_handle;
class yaml_parser_handle;

struct load_options
{
    // The body names the module's factory, so a caller's object file carries an undefined
    // reference the linker has to resolve and the archive member is pulled in without a static
    // registrar. The gate is a generated installed header rather than a compile definition,
    // because a definition set on a build target never reaches a consumer of the installed
    // package.
    load_options()
        : on_missing(missing_asset::fail),
          topology(topology_policy::fail),
          materials(material_policy::warn),
          strict(strictness::fail),
          eval(eval_policy::fail),
          backend(),
          yaml(),
          package_roots(),
          args()
    {
#ifdef MEIOS_HAS_YAML
        yaml = make_yaml_parser();
#endif
    }

    missing_asset   on_missing;
    topology_policy topology;
    material_policy materials;
    strictness      strict;
    eval_policy     eval;

    std::shared_ptr<evaluator_handle> backend;

    std::shared_ptr<const yaml_parser_handle> yaml;

    std::vector<std::filesystem::path> package_roots;

    // Caller overrides seeded into the evaluation scope before expansion; because
    // the declared-default seed is seed-if-absent, a name set here wins over the
    // document's <xacro:arg> default.
    std::map<std::string, std::string> args;
};

expected<load_result, load_error> load(const std::filesystem::path &path,
                                       const load_options &opts, log_sink &log);

// This overload takes the capture itself rather than a plain sink: a source the caller built
// reports through the sink it was constructed with, so unless that sink is this object the
// load cannot see its errors and cannot refuse on them. Build the sources against the same
// capture that is handed here.
expected<load_result, load_error> load(const std::filesystem::path &path,
                                       const load_options &opts, source_stack &sources,
                                       capturing_log_sink &log);

expected<load_result, load_error> load(const std::filesystem::path &path,
                                       const load_options &opts = {});

}

#endif

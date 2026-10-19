#ifndef HPP_GUARD_MEIOS_URDF_PARSE_CONTEXT_H
#define HPP_GUARD_MEIOS_URDF_PARSE_CONTEXT_H

#include "meios/urdf/policy.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/completeness.h"
#include "meios/diagnostic/missing_asset.h"
#include "meios/diagnostic/topology_policy.h"

#include "meios/xacro/include_options.h"

#include <vector>
#include <filesystem>

namespace meios
{

class source_stack;
class core_evaluator;

struct parse_context
{
    source_stack   &sources;
    core_evaluator &eval;
    log_sink        &log;
    missing_asset   on_missing;
    topology_policy topology;
    material_policy materials;
    strictness      strict;
    std::filesystem::path document;
    // A policy set to skip suppresses the diagnostic, never the drop it reports. The claims
    // are derived from the emitted diagnostics, so a suppressed one records what it would
    // have withdrawn here instead and the derivation withdraws it all the same.
    completeness withheld;
    // The directories a consumer granted authority over, which with the directory the input document
    // was named in and the one it really lives in are the only places an asset path may reach, unless
    // the caller follows outside links.
    std::vector<std::filesystem::path> package_roots;
    // The options a caller set for this load: follow_outside_links lets a relative asset beside the
    // document that is a link leading out of every place it may reach resolve where the link leads.
    // Left out of an initializer, both stay off.
    include_options includes;
};

}

#endif

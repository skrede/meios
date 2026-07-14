#ifndef HPP_GUARD_MEIOS_URDF_PARSE_CONTEXT_H
#define HPP_GUARD_MEIOS_URDF_PARSE_CONTEXT_H

#include "meios/urdf/policy.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/missing_asset.h"
#include "meios/diagnostic/topology_policy.h"

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
};

}

#endif

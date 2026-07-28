#include "verbs.h"

#include "meios/completion.h"

#include "meios/urdf/policy.h"
#include "meios/urdf/asset_uri.h"
#include "meios/urdf/parse_context.h"

#include "meios/xacro/arg_scan.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/completeness.h"
#include "meios/diagnostic/missing_asset.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/topology_policy.h"

#include <string>
#include <vector>
#include <iostream>
#include <optional>
#include <filesystem>

namespace meios::cli
{

namespace
{

// resolve_asset_uri reads the sources, the log, the missing-asset setting, the document and
// the configured roots, and nothing else; the topology, material and strictness settings
// belong to a parse this verb never runs, so their values here are inert.
parse_context resolution_context(source_stack &sources, core_evaluator &eval, log_sink &log,
                                 const verb_context &ctx,
                                 const std::vector<std::filesystem::path> &roots)
{
    return parse_context{ sources,
                          eval,
                          log,
                          missing_asset::fail,
                          topology_policy::fail,
                          material_policy::fail,
                          strictness::fail,
                          positional(ctx, 0),
                          completeness::none,
                          roots };
}

}

int run_resolve(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    // Nothing marks a position required, so a single argument binds to the model and leaves
    // the reference empty; saying so beats resolving "" and reporting it as an absent asset.
    if(positional(ctx, 1).empty())
    {
        log.log(level::error, "resolve takes a model document and an asset reference");
        return 1;
    }
    const std::vector<std::filesystem::path> roots = to_paths(ctx.package_paths);
    source_stack sources = build_sources(roots, log);
    core_evaluator eval;
    parse_context resolution = resolution_context(sources, eval, log, ctx, roots);
    const std::optional<std::string> path =
        detail::resolve_asset_uri(positional(ctx, 1), resolution, source_location{});
    if(!path)
        return 1;
    std::cout << *path << '\n';
    return 0;
}

int run_args(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    const std::vector<arg_declaration> declared =
        scan_args(std::filesystem::path(positional(ctx, 0)), log);
    for(const arg_declaration &arg : declared)
    {
        std::cout << arg.name;
        if(arg.default_value)
            std::cout << " = " << *arg.default_value;
        std::cout << '\n';
    }
    return 0;
}

int run_completion(const verb_context &ctx)
{
    const completion_model surface = lower(cli_table());
    const std::string &shell = positional(ctx, 0);
    if(shell == "bash")
        std::cout << emit_bash(surface);
    else if(shell == "zsh")
        std::cout << emit_zsh(surface);
    else if(shell == "fish")
        std::cout << emit_fish(surface);
    else
    {
        log_sink_s log(std::cerr);
        log.log(level::error, "unknown shell: " + shell + " (expected bash|zsh|fish)");
        return 1;
    }
    return 0;
}

}

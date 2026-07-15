#include "verbs.h"
#include "counting_log_sink.h"
#include "register_scanners.h"

#include "meios/urdf/load.h"

#include "meios/xacro/evaluator_handle.h"

#ifdef MEIOS_CLI_HAS_EVAL_PYTHON
#include "meios/eval/python_evaluator.h"
#endif

#include "meios/bundle/flatten.h"
#include "meios/bundle/manifest.h"
#include "meios/bundle/package_writer.h"
#include "meios/bundle/scanner_registry.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <map>
#include <string>
#include <vector>
#include <ostream>
#include <iostream>
#include <optional>
#include <filesystem>

namespace meios::cli
{

bool ros_linked()
{
#ifdef MEIOS_CLI_HAS_ROS
    return true;
#else
    return false;
#endif
}

bool eval_python_linked()
{
#ifdef MEIOS_CLI_HAS_EVAL_PYTHON
    return true;
#else
    return false;
#endif
}

eval_policy eval_policy_of(const verb_context &ctx)
{
    const auto policy = ctx.value_flags.find("--eval-policy");
    if(policy == ctx.value_flags.end())
        return eval_policy::fail;
    if(policy->second == "warn")
        return eval_policy::warn;
    if(policy->second == "skip")
        return eval_policy::skip;
    return eval_policy::fail;
}

// Binds a python-backed handle when --eval python is asked for, holding it in the
// caller's named local so opts.backend never outlives its storage; a request with
// the backend unlinked fails loudly rather than downgrading to the core evaluator.
bool select_backend(const verb_context &ctx, load_options &opts,
                    std::optional<evaluator_handle> &handle, log_sink &log)
{
    const auto eval = ctx.value_flags.find("--eval");
    if(eval == ctx.value_flags.end() || eval->second != "python")
        return true;
    if(!eval_python_linked())
    {
        log.log(level::error, "the python evaluator backend is not built into this binary; "
                              "rebuild with the eval-python enrichment to use --eval python");
        return false;
    }
    (void)opts;
    (void)handle;
#ifdef MEIOS_CLI_HAS_EVAL_PYTHON
    handle.emplace(python_evaluator{});
    opts.backend = &*handle;
#endif
    return true;
}

bool collect_arg_overrides(const std::vector<std::string> &tokens,
                           std::map<std::string, std::string> &args, log_sink &log)
{
    bool ok = true;
    for(const std::string &token : tokens)
    {
        const std::string::size_type split = token.find(":=");
        if(split == std::string::npos || split == 0)
        {
            log.log(level::error, "malformed argument override '" + token
                + "'; expected key:=value with a non-empty key");
            ok = false;
            continue;
        }
        args[token.substr(0, split)] = token.substr(split + 2);
    }
    return ok;
}

int run_flatten(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    counting_log_sink sink(log);
    load_options opts;
    opts.package_roots = to_paths(ctx.package_paths);
    opts.eval = eval_policy_of(ctx);
    std::optional<evaluator_handle> handle;
    if(!collect_arg_overrides(ctx.arg_overrides, opts.args, sink)
       || !select_backend(ctx, opts, handle, log))
        return 1;
    source_stack sources = build_sources(opts.package_roots, sink);
    const model<double> robot = load(positional(ctx, 0), opts, sources, sink);
    if(sink.errors() != 0)
        return 1;
    const emit_result result = flatten(robot, std::cout, log);
    return result.status == emit_status::ok ? 0 : 1;
}

int run_bundle(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    counting_log_sink sink(log);
    const auto name = ctx.value_flags.find("--name");
    if(name == ctx.value_flags.end() || name->second.empty())
    {
        log.log(level::error, "bundle requires --name");
        return 1;
    }
    load_options opts;
    opts.package_roots = to_paths(ctx.package_paths);
    source_stack sources = build_sources(opts.package_roots, sink);
    const model<double> robot = load(positional(ctx, 0), opts, sources, sink);
    if(sink.errors() != 0)
        return 1;
    scanner_registry registry;
    register_scanners(registry);
    emit_result out{};
    const folder_request request{ std::filesystem::current_path() / name->second, name->second,
                                  collision_options{ false }, false };
    const asset_manifest manifest = bundle_to_folder(robot, sources, registry, request, out, log);
    return (out.status == emit_status::ok && manifest.unresolved.empty()) ? 0 : 1;
}

int run_deps(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    counting_log_sink sink(log);
    load_options opts;
    opts.package_roots = to_paths(ctx.package_paths);
    source_stack sources = build_sources(opts.package_roots, sink);
    const model<double> robot = load(positional(ctx, 0), opts, sources, sink);
    if(sink.errors() != 0)
        return 1;
    scanner_registry registry;
    register_scanners(registry);
    emit_result out{};
    const folder_request request{ std::filesystem::current_path(),
                                  std::filesystem::path(positional(ctx, 0)).stem().string(),
                                  collision_options{ false }, true };
    const asset_manifest manifest = bundle_to_folder(robot, sources, registry, request, out, log);
    for(const bundle_entry &entry : manifest.entries)
        std::cout << entry.copy_source.string() << '\n';
    return manifest.unresolved.empty() ? 0 : 1;
}

}

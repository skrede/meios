#include "verbs.h"
#include "register_scanners.h"

#include "meios/urdf/load.h"

#include "meios/bundle/flatten.h"
#include "meios/bundle/manifest.h"
#include "meios/bundle/package_writer.h"
#include "meios/bundle/scanner_registry.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <map>
#include <string>
#include <ostream>
#include <iostream>
#include <filesystem>

namespace meios::cli
{

int run_flatten(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    load_options opts;
    opts.package_roots = to_paths(ctx.package_paths);
    const model<double> robot = load(positional(ctx, 0), opts, log);
    const emit_result result = flatten(robot, std::cout, log);
    return result.status == emit_status::ok ? 0 : 1;
}

int run_bundle(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    const std::map<std::string, std::string>::const_iterator name = ctx.value_flags.find("--name");
    if(name == ctx.value_flags.end() || name->second.empty())
    {
        log.log(level::error, "bundle requires --name");
        return 1;
    }
    load_options opts;
    opts.package_roots = to_paths(ctx.package_paths);
    source_stack sources = build_sources(opts.package_roots, log);
    const model<double> robot = load(positional(ctx, 0), opts, sources, log);
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
    load_options opts;
    opts.package_roots = to_paths(ctx.package_paths);
    source_stack sources = build_sources(opts.package_roots, log);
    const model<double> robot = load(positional(ctx, 0), opts, sources, log);
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

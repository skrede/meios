#include "verbs.h"

#include "meios/completion.h"

#include "meios/xacro/arg_scan.h"

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>
#include <cstddef>
#include <iostream>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::cli
{

namespace
{

// Splits a `package://<pkg>/<rel>` (or bare `<pkg>/<rel>`) reference into its two
// tokens. This only parses the argument; the containment guard that keeps the
// resolved path inside its root stays in directory_source, reached via locate().
bool split_reference(const std::string &target, std::string &pkg, std::string &rel)
{
    std::string_view ref = target;
    constexpr std::string_view scheme = "package://";
    if(ref.substr(0, scheme.size()) == scheme)
        ref.remove_prefix(scheme.size());
    const std::size_t slash = ref.find('/');
    if(slash == std::string_view::npos || slash == 0 || slash + 1 >= ref.size())
        return false;
    pkg = std::string(ref.substr(0, slash));
    rel = std::string(ref.substr(slash + 1));
    return true;
}

}

int run_resolve(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    std::string pkg;
    std::string rel;
    if(!split_reference(positional(ctx, 1), pkg, rel))
    {
        log.log(level::error, "resolve target must be package://<pkg>/<rel>");
        return 1;
    }
    source_stack sources = build_sources(to_paths(ctx.package_paths), log);
    const std::optional<resolved_asset> hit = sources.locate(pkg, rel, log);
    if(!hit)
        return 1;
    std::cout << hit->path().string() << '\n';
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

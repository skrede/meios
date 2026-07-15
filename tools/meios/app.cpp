#include "app.h"
#include "complete.h"
#include "verbs/verbs.h"

#include "meios/completion/command_table.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <CLI/CLI.hpp>

#include <deque>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <iostream>
#include <string_view>

namespace meios::cli
{

namespace
{

int run_verb(const verb_context &ctx)
{
    if(ctx.id == "flatten")    return run_flatten(ctx);
    if(ctx.id == "bundle")     return run_bundle(ctx);
    if(ctx.id == "deps")       return run_deps(ctx);
    if(ctx.id == "info")       return run_info(ctx);
    if(ctx.id == "resolve")    return run_resolve(ctx);
    if(ctx.id == "validate")   return run_validate(ctx);
    if(ctx.id == "tree")       return run_tree(ctx);
    if(ctx.id == "args")       return run_args(ctx);
    if(ctx.id == "completion") return run_completion(ctx);
    log_sink_s log(std::cerr);
    log.log(level::error, "verb not yet implemented: " + ctx.id);
    return 2;
}

void add_flags(CLI::App &sub, const command_spec &spec, verb_context &ctx)
{
    for(const flag_spec &flag : spec.flags)
    {
        if(flag.token == "--package-path")
            sub.add_option(flag.token, ctx.package_paths, flag.description)->allow_extra_args(false);
        else if(flag.kind == flag_kind::boolean)
            sub.add_flag(flag.token, ctx.bool_flags[flag.token], flag.description);
        else
            sub.add_option(flag.token, ctx.value_flags[flag.token], flag.description);
    }
}

void add_positionals(CLI::App &sub, const command_spec &spec, verb_context &ctx)
{
    ctx.positionals.resize(spec.positionals.size());
    for(std::size_t i = 0; i < spec.positionals.size(); ++i)
    {
        const positional_spec &pos = spec.positionals[i];
        if(pos.variadic)
            sub.add_option(pos.name, ctx.arg_overrides, pos.description);
        else
            sub.add_option(pos.name, ctx.positionals[i], pos.description);
    }
}

void build(CLI::App &app, std::deque<verb_context> &contexts, int &exit_code)
{
    app.require_subcommand(1);
    for(const command_spec &spec : cli_table())
    {
        verb_context &ctx = contexts.emplace_back();
        ctx.id = spec.id;
        CLI::App *sub = app.add_subcommand(spec.name, spec.description);
        add_flags(*sub, spec, ctx);
        add_positionals(*sub, spec, ctx);
        sub->callback([&ctx, &exit_code] { exit_code = run_verb(ctx); });
    }
}

}

int run(int argc, char **argv)
{
    if(argc >= 2 && std::string_view(argv[1]) == "__complete")
        return run_complete(std::vector<std::string>(argv + 2, argv + argc));

    CLI::App app{ "meios URDF/xacro command-line frontend", "meios" };
    std::deque<verb_context> contexts;
    int exit_code = 0;
    build(app, contexts, exit_code);
    CLI11_PARSE(app, argc, argv);
    return exit_code;
}

std::vector<parser_surface_entry> registered_surface()
{
    CLI::App app{ "", "meios" };
    std::deque<verb_context> contexts;
    int exit_code = 0;
    build(app, contexts, exit_code);
    std::vector<parser_surface_entry> surface;
    // The no-filter get_subcommands() returns only parsed subcommands; a true filter
    // returns every registered one, which is the surface the drift test compares.
    for(const CLI::App *sub : app.get_subcommands([](const CLI::App *) { return true; }))
    {
        parser_surface_entry entry;
        entry.name = sub->get_name();
        for(const CLI::Option *opt : sub->get_options())
        {
            if(opt == sub->get_help_ptr())
                continue;
            (opt->get_positional() ? entry.positionals : entry.flags).push_back(opt->get_name());
        }
        surface.push_back(std::move(entry));
    }
    return surface;
}

}

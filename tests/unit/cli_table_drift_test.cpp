#include "app.h"

#include "meios/completion.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

using namespace meios;

namespace
{

std::set<std::string> option_tokens(const completion_verb &verb)
{
    std::set<std::string> tokens;
    for(const completion_option &option : verb.options)
        tokens.insert(option.token);
    return tokens;
}

std::set<std::string> argument_names(const completion_verb &verb)
{
    std::set<std::string> names;
    for(const completion_argument &argument : verb.arguments)
        names.insert(argument.name);
    return names;
}

}

TEST_CASE("cli_table_drift: parser surface equals the completion surface")
{
    const std::vector<cli::parser_surface_entry> parser = cli::registered_surface();
    const completion_model completion = lower(cli_table());

    std::map<std::string, cli::parser_surface_entry> parser_by_name;
    for(const cli::parser_surface_entry &entry : parser)
        parser_by_name.emplace(entry.name, entry);
    std::map<std::string, completion_verb> completion_by_name;
    for(const completion_verb &verb : completion.verbs)
        completion_by_name.emplace(verb.name, verb);

    REQUIRE(parser_by_name.size() == parser.size());
    REQUIRE(parser_by_name.size() == completion_by_name.size());

    for(const std::pair<const std::string, cli::parser_surface_entry> &named : parser_by_name)
    {
        REQUIRE(completion_by_name.count(named.first) == 1);
        const completion_verb &verb = completion_by_name.at(named.first);
        const std::set<std::string> flags(named.second.flags.begin(), named.second.flags.end());
        const std::set<std::string> positionals(named.second.positionals.begin(),
                                                 named.second.positionals.end());
        REQUIRE(flags == option_tokens(verb));
        REQUIRE(positionals == argument_names(verb));
    }

    for(const std::pair<const std::string, completion_verb> &named : completion_by_name)
        REQUIRE(parser_by_name.count(named.first) == 1);
}

TEST_CASE("cli_table_drift: the completion verb is part of the shared surface")
{
    const std::vector<cli::parser_surface_entry> parser = cli::registered_surface();
    bool found = false;
    for(const cli::parser_surface_entry &entry : parser)
        found = found || entry.name == "completion";
    REQUIRE(found);
}

TEST_CASE("cli_table_drift: resolve names its package reference and deps carries the override surface")
{
    const std::vector<command_spec> &table = cli_table();

    const command_spec *resolve = nullptr;
    const command_spec *deps = nullptr;
    for(const command_spec &spec : table)
    {
        if(spec.name == "resolve")
            resolve = &spec;
        if(spec.name == "deps")
            deps = &spec;
    }
    REQUIRE(resolve != nullptr);
    REQUIRE(deps != nullptr);

    REQUIRE(resolve->description.find("package://") != std::string::npos);
    REQUIRE(resolve->description.find("link or joint") == std::string::npos);

    const positional_spec *target = nullptr;
    for(const positional_spec &positional : resolve->positionals)
        if(positional.name == "target")
            target = &positional;
    REQUIRE(target != nullptr);
    REQUIRE(target->description.find("package://") != std::string::npos);

    bool deps_has_eval = false;
    for(const flag_spec &flag : deps->flags)
        deps_has_eval = deps_has_eval || flag.token == "--eval";
    REQUIRE(deps_has_eval);

    bool deps_has_variadic_override = false;
    for(const positional_spec &positional : deps->positionals)
        deps_has_variadic_override = deps_has_variadic_override || positional.variadic;
    REQUIRE(deps_has_variadic_override);
}

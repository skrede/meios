#include "complete.h"
#include "verbs/verbs.h"
#include "verbs/label_escape.h"

#include "meios/completion/command_table.h"

#include "meios/urdf/load.h"

#include "meios/model/model.h"

#include "meios/records/link.h"

#include "meios/xacro/arg_scan.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>
#include <cstddef>
#include <iostream>
#include <filesystem>
#include <string_view>

namespace meios::cli
{

namespace
{

// Cobra shell-completion directives: 0 lets the shell add filesystem completion,
// 4 (NoFileComp) suppresses it after a semantic candidate set.
constexpr std::string_view directive_default = ":0\n";
constexpr std::string_view directive_no_file = ":4\n";

const command_spec *find_command(std::string_view verb)
{
    for(const command_spec &spec : cli_table())
        if(spec.name == verb)
            return &spec;
    return nullptr;
}

bool is_value_flag(const command_spec &spec, std::string_view token)
{
    for(const flag_spec &flag : spec.flags)
        if(flag.token == token)
            return flag.kind == flag_kind::value;
    return false;
}

std::vector<std::string> positionals_before_current(const std::vector<std::string> &words,
                                                    const command_spec *spec)
{
    std::vector<std::string> positionals;
    for(std::size_t i = 1; i + 1 < words.size();)
    {
        const std::string &token = words[i];
        if(token.rfind("--", 0) == 0)
            i += (spec != nullptr && is_value_flag(*spec, token)) ? std::size_t{ 2 } : std::size_t{ 1 };
        else
        {
            positionals.push_back(token);
            ++i;
        }
    }
    return positionals;
}

int emit(const std::vector<std::string> &candidates, std::string_view directive)
{
    for(const std::string &candidate : candidates)
        std::cout << escape_ascii(candidate) << '\n';
    std::cout << directive;
    return 0;
}

model<double> quiet_load(const std::string &model_path)
{
    log_sink quiet;
    load_options opts;
    // Completion offers link names and has no channel to report on; under the library's
    // refusing default an unresolved mesh would silently offer nothing at all.
    opts.on_missing = missing_asset::warn;
    const expected<load_result, load_error> loaded =
        load(std::filesystem::path(model_path), opts, quiet);
    return loaded ? loaded->robot : model<double>{};
}

int emit_links(const std::string &model_path)
{
    const model<double> robot = quiet_load(model_path);
    std::vector<std::string> names;
    for(const link<double> &node : robot.links)
        names.push_back(node.name);
    return emit(names, directive_no_file);
}

int emit_args(const std::string &model_path)
{
    log_sink quiet;
    std::vector<std::string> names;
    for(const arg_declaration &arg : scan_args(std::filesystem::path(model_path), quiet))
        names.push_back(arg.name);
    return emit(names, directive_no_file);
}

}

int run_complete(const std::vector<std::string> &argv)
{
    // The emitters pass the current word after a literal `--` boundary (the cobra
    // convention, quoting-safe); drop that separator so the prior real token drives
    // classification rather than the `--` itself.
    std::vector<std::string> words = argv;
    if(words.size() >= 2 && words[words.size() - 2] == "--")
        words.erase(words.end() - 2);

    if(words.size() < 2)
        return emit({}, directive_default);

    const std::string &verb = words[0];
    const command_spec *spec = find_command(verb);
    const std::string &previous = words[words.size() - 2];
    const std::vector<std::string> positionals = positionals_before_current(words, spec);
    const bool model_given = !positionals.empty();
    const bool at_second_positional = positionals.size() == 1;

    if(previous == "--root" && verb == "tree" && model_given)
        return emit_links(positionals.front());
    if(verb == "args" && at_second_positional)
        return emit_args(positionals.front());
    return emit({}, directive_default);
}

}

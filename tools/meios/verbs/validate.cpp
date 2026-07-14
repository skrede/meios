#include "verbs.h"
#include "validate.h"

#include <map>
#include <string>
#include <cstddef>
#include <iostream>

namespace meios::cli
{

int run_validate(const verb_context &ctx)
{
    const std::vector<std::filesystem::path> roots = to_paths(ctx.package_paths);
    const validation_report report = classify(positional(ctx, 0), roots);

    const std::map<std::string, std::string>::const_iterator format = ctx.value_flags.find("--format");
    const bool as_json = format != ctx.value_flags.end() && format->second == "json";
    std::cout << (as_json ? format_json(report) : format_text(report));
    return exit_code(report);
}

}

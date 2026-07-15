#ifndef HPP_GUARD_MEIOS_CLI_VERBS_H
#define HPP_GUARD_MEIOS_CLI_VERBS_H

#include "meios/io/source_stack.h"
#include "meios/io/source_handle.h"
#include "meios/io/directory_source.h"

#ifdef MEIOS_CLI_HAS_ROS
#include "meios/ros/ros_package_source.h"
#endif

#include "meios/diagnostic/log_sink.h"

#include <map>
#include <string>
#include <vector>
#include <filesystem>

namespace meios::cli
{

// True only in the meios_cli translation units, which alone carry the
// MEIOS_CLI_HAS_ROS guard; a test binary links the archive and reads this to skip
// the ros-dependent path when the enrichment was not built in.
bool ros_linked();

// The parsed argv for one verb, filled generically by the table-driven registration
// and read by the verb bodies. It carries no CLI11 type, so a verb body is a plain
// function the tests drive directly without the parser.
struct verb_context
{
    std::string id;
    std::vector<std::string> positionals;
    std::vector<std::string> package_paths;
    std::map<std::string, std::string> value_flags;
    std::map<std::string, bool> bool_flags;
};

inline std::vector<std::filesystem::path> to_paths(const std::vector<std::string> &roots)
{
    return std::vector<std::filesystem::path>(roots.begin(), roots.end());
}

inline source_stack build_sources(const std::vector<std::filesystem::path> &roots, log_sink &log)
{
    source_stack sources;
    for(const std::filesystem::path &root : roots)
        sources.push_back(source_handle(directory_source(root, log)));
#ifdef MEIOS_CLI_HAS_ROS
    // Explicit --package-path roots keep highest precedence as literal directory
    // sources; the ros layers follow, resolving by package.xml <name> across a
    // recursive crawl of those roots, then the ROS2 ament index and ROS1
    // ROS_PACKAGE_PATH taken from the environment.
    sources.push_back(source_handle(ros_package_source(roots, {}, log)));
    sources.push_back(source_handle(ros_package_source::from_environment(log)));
#endif
    return sources;
}

inline const std::string &positional(const verb_context &ctx, std::size_t index)
{
    static const std::string empty;
    return index < ctx.positionals.size() ? ctx.positionals[index] : empty;
}

int run_flatten(const verb_context &ctx);

int run_bundle(const verb_context &ctx);

int run_deps(const verb_context &ctx);

int run_info(const verb_context &ctx);

int run_resolve(const verb_context &ctx);

int run_validate(const verb_context &ctx);

int run_tree(const verb_context &ctx);

int run_args(const verb_context &ctx);

int run_completion(const verb_context &ctx);

}

#endif

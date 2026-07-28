#include "verbs.h"
#include "tree.h"
#include "deferred_error_sink.h"

#include "meios/urdf/load.h"

#include "meios/model/topology.h"

#include "meios/records/link.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <map>
#include <string>
#include <vector>
#include <cstddef>
#include <iostream>
#include <string_view>

namespace meios::cli
{

std::string_view joint_kind_label(joint_kind kind)
{
    switch(kind)
    {
        case joint_kind::fixed:      return "fixed";
        case joint_kind::revolute:   return "revolute";
        case joint_kind::continuous: return "continuous";
        case joint_kind::prismatic:  return "prismatic";
        case joint_kind::floating:   return "floating";
        case joint_kind::planar:     return "planar";
    }
    return "unknown";
}

std::vector<std::size_t> roots_of(const std::vector<int> &parent_of)
{
    std::vector<std::size_t> roots;
    for(std::size_t i = 0; i < parent_of.size(); ++i)
        if(parent_of[i] == -1)
            roots.push_back(i);
    return roots;
}

child_map build_children(const model<double> &robot, const std::vector<int> &parent_of)
{
    child_map children(robot.links.size());
    for(std::size_t j = 0; j < robot.joints.size(); ++j)
    {
        const std::unordered_map<std::string, int>::const_iterator ci =
            robot.link_index.find(robot.joints[j].child);
        const std::unordered_map<std::string, int>::const_iterator pi =
            robot.link_index.find(robot.joints[j].parent);
        if(ci == robot.link_index.end() || pi == robot.link_index.end())
            continue;
        if(parent_of[static_cast<std::size_t>(ci->second)] == pi->second)
            children[static_cast<std::size_t>(pi->second)].push_back({ static_cast<std::size_t>(ci->second), j });
    }
    return children;
}

int render_rooted(const model<double> &robot, const std::vector<int> &parent_of,
                  const std::string &root_name, bool dot, log_sink &log)
{
    if(dot)
    {
        log.log(level::error, "--root is not supported together with --dot");
        return 1;
    }
    const auto entry = robot.link_index.find(root_name);
    if(entry == robot.link_index.end())
    {
        log.log(level::error, "unknown link: " + root_name);
        return 1;
    }
    std::cout << render_ascii(robot, build_children(robot, parent_of),
                              { static_cast<std::size_t>(entry->second) });
    return 0;
}

int run_tree(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    deferred_error_sink sink(log);
    capturing_log_sink capture(sink);
    load_options opts;
    opts.topology = topology_policy::warn;
    // The tree renders kinematics and never an asset, so an unresolved mesh must not
    // withhold the drawing the caller asked for; the library default would refuse.
    opts.on_missing = missing_asset::warn;
    opts.package_roots = to_paths(ctx.package_paths);
    source_stack sources = build_sources(opts.package_roots, capture);
    const expected<load_result, load_error> loaded =
        load(positional(ctx, 0), opts, sources, capture);
    if(!loaded)
    {
        log.log(level::error, loaded.error().code, loaded.error().loc, loaded.error().message);
        return 1;
    }
    if(sink.errors() != 0)
    {
        sink.replay_first(log);
        return 1;
    }
    const model<double> &robot = loaded->robot;

    log_sink quiet;
    const topology_result topo =
        reconstruct_topology(robot.links, robot.joints, quiet, topology_policy::skip);

    const bool dot = ctx.bool_flags.count("--dot") != 0 && ctx.bool_flags.at("--dot");
    const auto root = ctx.value_flags.find("--root");
    if(root != ctx.value_flags.end() && !root->second.empty())
        return render_rooted(robot, topo.topo.parent_of, root->second, dot, log);

    if(dot)
        std::cout << render_dot(robot);
    else
        std::cout << render_ascii(robot, build_children(robot, topo.topo.parent_of), roots_of(topo.topo.parent_of));
    return 0;
}

}

#include "verbs.h"
#include "tree.h"

#include "meios/urdf/load.h"

#include "meios/model/topology.h"

#include "meios/records/link.h"

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

int run_tree(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    load_options opts;
    opts.topology = topology_policy::warn;
    opts.package_roots = to_paths(ctx.package_paths);
    const model<double> robot = load(positional(ctx, 0), opts, log);

    log_sink quiet;
    const topology_result topo =
        reconstruct_topology(robot.links, robot.joints, quiet, topology_policy::skip);

    if(ctx.bool_flags.count("--dot") != 0 && ctx.bool_flags.at("--dot"))
        std::cout << render_dot(robot);
    else
        std::cout << render_ascii(robot, build_children(robot, topo.parent_of), roots_of(topo.parent_of));
    return 0;
}

}

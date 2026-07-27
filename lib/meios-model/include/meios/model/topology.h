#ifndef HPP_GUARD_MEIOS_MODEL_MODEL_TOPOLOGY_H
#define HPP_GUARD_MEIOS_MODEL_MODEL_TOPOLOGY_H

#include "meios/records/link.h"
#include "meios/records/joint.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/topology_policy.h"

#include <string>
#include <vector>
#include <cstddef>

namespace meios
{

struct robot_topology
{
    std::vector<int> parent_of;
    std::vector<int> joint_of;
    std::vector<int> roots;
    std::vector<int> order;
};

struct topology_result
{
    robot_topology topo;
    bool ok;
};

namespace detail
{

template <typename Links>
int index_of(const Links &links, const std::string &name)
{
    for(std::size_t i = 0; i < links.size(); ++i)
        if(links[i].name == name)
            return static_cast<int>(i);
    return -1;
}

template <typename Record>
void report_topology(log_sink &log, topology_policy policy, const Record &rec,
                     diagnostic_code code, const std::string &message, bool &ok)
{
    if(policy == topology_policy::skip)
        return;
    const level lvl = policy == topology_policy::fail ? level::error : level::warn;
    if(rec.origin_loc)
        log.log(lvl, code, *rec.origin_loc, message);
    else
        log.log(lvl, code, source_location{}, message);
    if(policy == topology_policy::fail)
        ok = false;
}

template <typename Scalar>
void assign_parents(const std::vector<link<Scalar>> &links,
                    const std::vector<joint<Scalar>> &joints, std::vector<int> &parent_of,
                    std::vector<int> &joint_of, log_sink &log, topology_policy policy, bool &ok)
{
    for(std::size_t j = 0; j < joints.size(); ++j)
    {
        const joint<Scalar> &edge = joints[j];
        const int pi = index_of(links, edge.parent);
        const int ci = index_of(links, edge.child);
        // The load rules refuse an undeclared link reference before a record exists, so an
        // unresolved index here can only come from a model assembled outside them: an
        // invariant violation rather than a graph property, which no policy setting grades.
        if(pi < 0 || ci < 0)
        {
            log.log(level::error, diagnostic_code::invalid_topology, source_location{},
                    "joint '" + edge.name + "' names a link absent from the model");
            ok = false;
        }
        else if(parent_of[static_cast<std::size_t>(ci)] != -1)
            report_topology(log, policy, edge, diagnostic_code::multiple_parents,
                "link '" + edge.child + "' has more than one parent joint", ok);
        else
        {
            parent_of[static_cast<std::size_t>(ci)] = pi;
            joint_of[static_cast<std::size_t>(ci)] = static_cast<int>(j);
        }
    }
}

template <typename Scalar>
std::vector<int> collect_roots(const std::vector<link<Scalar>> &links,
                               const std::vector<int> &parent_of, log_sink &log,
                               topology_policy policy, bool &ok)
{
    std::vector<int> roots;
    for(std::size_t i = 0; i < parent_of.size(); ++i)
        if(parent_of[i] == -1)
            roots.push_back(static_cast<int>(i));
    for(std::size_t r = 1; r < roots.size(); ++r)
        report_topology(log, policy, links[static_cast<std::size_t>(roots[r])], diagnostic_code::additional_root,
            "link '" + links[static_cast<std::size_t>(roots[r])].name + "' is an additional root", ok);
    if(roots.empty() && !links.empty())
        report_topology(log, policy, links.front(), diagnostic_code::no_root_cycle,
            "no root link; the graph has a cycle", ok);
    return roots;
}

template <typename Scalar>
void detect_cycles(const std::vector<link<Scalar>> &links, const std::vector<int> &parent_of,
                   log_sink &log, topology_policy policy, bool &ok)
{
    const std::size_t n = parent_of.size();
    for(std::size_t i = 0; i < n; ++i)
    {
        std::vector<bool> seen(n, false);
        for(int cur = static_cast<int>(i); cur != -1; cur = parent_of[static_cast<std::size_t>(cur)])
        {
            if(seen[static_cast<std::size_t>(cur)])
            {
                report_topology(log, policy, links[i], diagnostic_code::link_on_cycle,
                    "link '" + links[i].name + "' lies on a cycle", ok);
                break;
            }
            seen[static_cast<std::size_t>(cur)] = true;
        }
    }
}

inline void push_children(const std::vector<int> &parent_of, int cur, std::vector<int> &stack)
{
    std::vector<int> kids;
    for(std::size_t i = 0; i < parent_of.size(); ++i)
        if(parent_of[i] == cur)
            kids.push_back(static_cast<int>(i));
    // push children descending so the LIFO pops them ascending, giving pre-order ascending children
    for(std::vector<int>::const_reverse_iterator it = kids.rbegin(); it != kids.rend(); ++it)
        stack.push_back(*it);
}

inline std::vector<int> build_order(const std::vector<int> &parent_of, const std::vector<int> &roots)
{
    std::vector<int> order;
    order.reserve(parent_of.size());
    std::vector<bool> seen(parent_of.size(), false);
    std::vector<int> stack(roots.rbegin(), roots.rend());
    while(!stack.empty())
    {
        const int cur = stack.back();
        stack.pop_back();
        if(seen[static_cast<std::size_t>(cur)])
            continue;
        seen[static_cast<std::size_t>(cur)] = true;
        order.push_back(cur);
        push_children(parent_of, cur, stack);
    }
    return order;
}

template <typename Scalar>
void report_unreachable(const std::vector<link<Scalar>> &links, const std::vector<int> &order,
                        log_sink &log, topology_policy policy, diagnostic_code code, bool &ok)
{
    std::vector<bool> seen(links.size(), false);
    for(const int idx : order)
        seen[static_cast<std::size_t>(idx)] = true;
    for(std::size_t i = 0; i < links.size(); ++i)
        if(!seen[i])
            report_topology(log, policy, links[i], code,
                "link '" + links[i].name + "' is unreachable from the root", ok);
}

}

template <typename Scalar>
topology_result reconstruct_topology(const std::vector<link<Scalar>> &links,
                                     const std::vector<joint<Scalar>> &joints, log_sink &log,
                                     topology_policy policy)
{
    const std::size_t n = links.size();
    topology_result result{ robot_topology{ std::vector<int>(n, -1), std::vector<int>(n, -1), {}, {} }, true };
    detail::assign_parents(links, joints, result.topo.parent_of, result.topo.joint_of, log, policy, result.ok);
    result.topo.roots = detail::collect_roots(links, result.topo.parent_of, log, policy, result.ok);
    detail::detect_cycles(links, result.topo.parent_of, log, policy, result.ok);
    result.topo.order = detail::build_order(result.topo.parent_of, result.topo.roots);
    if(!result.topo.roots.empty())
        detail::report_unreachable(links, result.topo.order, log, policy,
                                   diagnostic_code::unreachable_link, result.ok);
    return result;
}

}

#endif

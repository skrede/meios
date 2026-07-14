#ifndef HPP_GUARD_MEIOS_MODEL_MODEL_TOPOLOGY_H
#define HPP_GUARD_MEIOS_MODEL_MODEL_TOPOLOGY_H

#include "meios/records/link.h"
#include "meios/records/joint.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/topology_policy.h"

#include <string>
#include <vector>
#include <cstddef>

namespace meios
{

struct topology_result
{
    std::vector<int> parent_of;
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
                     const std::string &message, bool &ok)
{
    if(policy == topology_policy::skip)
        return;
    const level lvl = policy == topology_policy::fail ? level::error : level::warn;
    if(rec.origin_loc)
        log.log(lvl, *rec.origin_loc, message);
    else
        log.log(lvl, message);
    if(policy == topology_policy::fail)
        ok = false;
}

template <typename Scalar>
void assign_parents(const std::vector<link<Scalar>> &links,
                    const std::vector<joint<Scalar>> &joints, std::vector<int> &parent_of,
                    log_sink &log, topology_policy policy, bool &ok)
{
    for(const auto &edge : joints)
    {
        const int pi = index_of(links, edge.parent);
        const int ci = index_of(links, edge.child);
        if(pi < 0 || ci < 0)
            report_topology(log, policy, edge,
                "joint '" + edge.name + "' names an undeclared link", ok);
        else if(parent_of[static_cast<std::size_t>(ci)] != -1)
            report_topology(log, policy, edge,
                "link '" + edge.child + "' has more than one parent joint", ok);
        else
            parent_of[static_cast<std::size_t>(ci)] = pi;
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
        report_topology(log, policy, links[static_cast<std::size_t>(roots[r])],
            "link '" + links[static_cast<std::size_t>(roots[r])].name + "' is an additional root", ok);
    if(roots.empty() && !links.empty())
        report_topology(log, policy, links.front(), "no root link; the graph has a cycle", ok);
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
                report_topology(log, policy, links[i], "link '" + links[i].name + "' lies on a cycle", ok);
                break;
            }
            seen[static_cast<std::size_t>(cur)] = true;
        }
    }
}

template <typename Scalar>
void detect_unreachable(const std::vector<link<Scalar>> &links, const std::vector<int> &parent_of,
                        const std::vector<int> &roots, log_sink &log, topology_policy policy, bool &ok)
{
    const std::size_t n = parent_of.size();
    if(roots.empty())
        return;
    std::vector<bool> reached(n, false);
    for(std::vector<int> stack{ roots.front() }; !stack.empty();)
    {
        const int cur = stack.back();
        stack.pop_back();
        if(reached[static_cast<std::size_t>(cur)])
            continue;
        reached[static_cast<std::size_t>(cur)] = true;
        for(std::size_t i = 0; i < n; ++i)
            if(parent_of[i] == cur)
                stack.push_back(static_cast<int>(i));
    }
    for(std::size_t i = 0; i < n; ++i)
        if(!reached[i])
            report_topology(log, policy, links[i],
                "link '" + links[i].name + "' is unreachable from the root", ok);
}

}

template <typename Scalar>
topology_result reconstruct_topology(const std::vector<link<Scalar>> &links,
                                     const std::vector<joint<Scalar>> &joints, log_sink &log,
                                     topology_policy policy)
{
    topology_result result{ std::vector<int>(links.size(), -1), true };
    detail::assign_parents(links, joints, result.parent_of, log, policy, result.ok);
    const std::vector<int> roots = detail::collect_roots(links, result.parent_of, log, policy, result.ok);
    detail::detect_cycles(links, result.parent_of, log, policy, result.ok);
    detail::detect_unreachable(links, result.parent_of, roots, log, policy, result.ok);
    return result;
}

}

#endif

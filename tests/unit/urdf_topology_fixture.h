#ifndef HPP_GUARD_MEIOS_UNIT_URDF_TOPOLOGY_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_URDF_TOPOLOGY_FIXTURE_H

#include <meios/model.h>

#include <meios/model/topology.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <string>
#include <vector>
#include <cstdint>

namespace
{

struct entry
{
    meios::level lvl;
    meios::diagnostic_code code;
    std::int32_t line;
    std::string msg;
};

struct capture
{
    std::vector<entry> &entries;

    void operator()(meios::level lvl, const std::string &msg)
    {
        entries.push_back({lvl, meios::diagnostic_code::unspecified, 0, msg});
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &msg)
    {
        entries.push_back({lvl, meios::diagnostic_code::unspecified, loc.line, msg});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc, const std::string &msg)
    {
        entries.push_back({lvl, code, loc.line, msg});
    }
};

inline meios::topology_result reconstruct(const meios::tree<double> &robot, std::vector<entry> &entries, meios::topology_policy policy)
{
    meios::log_sink_f cap{capture{entries}};
    return meios::reconstruct_topology(robot.links, robot.joints, cap, policy);
}

}

#endif

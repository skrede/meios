#include "validate.h"

#include "meios/records/joint.h"

#include "meios/diagnostic/level.h"

#include <string>

namespace meios::cli
{

namespace
{

bool requires_limit(joint_kind kind)
{
    return kind == joint_kind::revolute || kind == joint_kind::prismatic;
}

void report_missing_limit(const joint<double> &edge, log_sink &log)
{
    const std::string message =
        "joint '" + edge.name + "' of a bounded kind must declare a <limit>";
    if(edge.origin_loc)
        log.log(level::error, *edge.origin_loc, message);
    else
        log.log(level::error, message);
}

}

void schema_check(const model<double> &robot, log_sink &log)
{
    for(const joint<double> &edge : robot.joints)
        if(requires_limit(edge.kind) && !edge.limits)
            report_missing_limit(edge, log);
}

}

#include "model_facts.h"

#include <meios/model.h>

#include <map>
#include <string>
#include <vector>
#include <utility>
#include <optional>

namespace consumer
{

namespace
{

int check_field(const std::string &key, const std::vector<double> &recorded, double loaded)
{
    if(recorded.size() != 1)
        return refuse_fact(key, "one number", text_of(loaded));
    if(same_number(recorded.front(), loaded))
        return 0;
    return refuse_fact(key, text_of(recorded.front()), text_of(loaded));
}

// Only the keys the recorded row carries are compared: upstream emits a limit element with no
// position keys for a continuous joint, and a joint it gives no limit at all must load without
// one, which the presence comparison below holds in both directions.
int check_recorded(const std::string &key, const std::string &recorded,
                   const meios::joint_limits<double> &limits)
{
    const std::map<std::string, double> loaded{ { "lower", limits.lower },
                                                { "upper", limits.upper },
                                                { "effort", limits.effort },
                                                { "velocity", limits.velocity } };
    for(const std::pair<const std::string, std::vector<double>> &field : keyed_numbers(recorded))
    {
        const std::map<std::string, double>::const_iterator at = loaded.find(field.first);
        if(at == loaded.end())
            return refuse_fact(key, field.first, "no limit field of that name");
        if(const int rc = check_field(key + " " + field.first, field.second, at->second))
            return rc;
    }
    return 0;
}

}

int check_limits(const meios::model<double> &robot, const std::vector<oracle::row> &rows)
{
    for(const meios::joint<double> &one : robot.joints)
    {
        const std::string key = "joint.limit." + one.name;
        const std::optional<oracle::row> found = oracle::lookup(rows, key);
        if(found.has_value() != one.limits.has_value())
            return refuse_fact(key, found.has_value() ? "a limit" : "no limit",
                               one.limits.has_value() ? "a limit" : "no limit");
        if(!found.has_value() || found->fields.size() < 2)
            continue;
        if(const int rc = check_recorded(key, found->fields[1], *one.limits))
            return rc;
    }
    return 0;
}

}

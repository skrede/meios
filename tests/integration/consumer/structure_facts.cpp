#include "model_facts.h"

#include <meios/model.h>

#include <string>
#include <vector>
#include <cstddef>

namespace consumer
{

namespace
{

std::string kind_text(meios::joint_kind kind)
{
    switch(kind)
    {
        case meios::joint_kind::revolute:   return "revolute";
        case meios::joint_kind::continuous: return "continuous";
        case meios::joint_kind::prismatic:  return "prismatic";
        case meios::joint_kind::floating:   return "floating";
        case meios::joint_kind::planar:     return "planar";
        case meios::joint_kind::fixed:      return "fixed";
    }
    return "unrecognized";
}

template <typename Item>
int check_roster(const std::vector<oracle::row> &rows, const std::string &prefix,
                 const std::vector<Item> &items)
{
    if(const int rc = check_text(rows, prefix + ".count", std::to_string(items.size())))
        return rc;
    for(std::size_t at = 0; at < items.size(); ++at)
        if(const int rc = check_text(rows, prefix + ".name." + std::to_string(at), items[at].name))
            return rc;
    return 0;
}

}

int check_structure(const meios::model<double> &robot, const std::vector<oracle::row> &rows)
{
    if(const int rc = check_roster(rows, "link", robot.links))
        return rc;
    if(const int rc = check_roster(rows, "joint", robot.joints))
        return rc;
    for(const meios::joint<double> &one : robot.joints)
        if(const int rc = check_text(rows, "joint.type." + one.name, kind_text(one.kind)))
            return rc;
    return 0;
}

}

#ifndef HPP_GUARD_MEIOS_BUNDLE_REPLAY_H
#define HPP_GUARD_MEIOS_BUNDLE_REPLAY_H

#include "meios/model/tree.h"
#include "meios/model/model.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/material.h"
#include "meios/records/robot_info.h"

namespace meios
{

template <typename Sink>
void replay(const tree<double> &robot, Sink &sink)
{
    sink.on_robot(robot_info{ robot.name, {} });
    for(const material<double> &mat : robot.materials)
        sink.on_material(mat);
    for(const link<double> &node : robot.links)
        sink.on_link(node);
    for(const joint<double> &edge : robot.joints)
        sink.on_joint(edge);
    sink.finish();
}

template <typename Sink>
void replay(const model<double> &robot, Sink &sink)
{
    sink.on_robot(robot_info{ robot.name, {} });
    for(const material<double> &mat : robot.materials)
        sink.on_material(mat);
    for(const link<double> &node : robot.links)
        sink.on_link(node);
    for(const joint<double> &edge : robot.joints)
        sink.on_joint(edge);
    sink.finish();
}

}

#endif

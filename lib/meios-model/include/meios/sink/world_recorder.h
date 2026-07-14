#ifndef HPP_GUARD_MEIOS_MODEL_SINK_WORLD_RECORDER_H
#define HPP_GUARD_MEIOS_MODEL_SINK_WORLD_RECORDER_H

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/material.h"
#include "meios/records/robot_info.h"

#include "meios/model/model.h"
#include "meios/model/topology.h"
#include "meios/model/structure.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/topology_policy.h"

namespace meios
{

class world_recorder
{
public:
    world_recorder(log_sink &log, topology_policy policy) : m_log(log), m_policy(policy), m_model() {}

    void on_robot(const robot_info &robot)
    {
        m_model.name = robot.name;
    }

    void on_material(const material<double> &mat)
    {
        m_model.materials.push_back(mat);
    }

    void on_link(const link<double> &node)
    {
        m_model.link_index[node.name] = static_cast<int>(m_model.links.size());
        m_model.links.push_back(node);
    }

    void on_joint(const joint<double> &edge)
    {
        m_model.joint_index[edge.name] = static_cast<int>(m_model.joints.size());
        m_model.joints.push_back(edge);
    }

    void finish()
    {
        reconstruct_topology(m_model.links, m_model.joints, m_log, m_policy);
        m_model.kind = m_model.loops.empty() ? structure::tree : structure::closed_chain;
    }

    const model<double> &result() const
    {
        return m_model;
    }

private:
    log_sink &m_log;
    topology_policy m_policy;
    model<double> m_model;
};

}

#endif

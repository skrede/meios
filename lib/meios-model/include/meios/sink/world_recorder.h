#ifndef HPP_GUARD_MEIOS_MODEL_SINK_WORLD_RECORDER_H
#define HPP_GUARD_MEIOS_MODEL_SINK_WORLD_RECORDER_H

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/material.h"
#include "meios/records/robot_info.h"

#include "meios/model/model.h"
#include "meios/model/topology.h"
#include "meios/model/structure.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/topology_policy.h"

#include <utility>
#include <optional>

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
        first_error_capture capture(m_log, m_first_failure);
        const topology_result topo =
            reconstruct_topology(m_model.links, m_model.joints, capture, m_policy);
        m_model.topo = topo.topo;
        m_ok = topo.ok;
        m_model.kind = m_model.loops.empty() ? structure::tree : structure::closed_chain;
    }

    bool ok() const
    {
        return m_ok;
    }

    const std::optional<source_location> &first_failure() const
    {
        return m_first_failure;
    }

    const model<double> &result() const
    {
        return m_model;
    }

    model<double> take_model()
    {
        return std::move(m_model);
    }

private:
    // Forwards every diagnostic to the real sink and, as a side effect, keeps the
    // source_location of the first located error so load() can report where a
    // topology failure originated without threading it through reconstruct_topology.
    class first_error_capture final : public log_sink
    {
    public:
        first_error_capture(log_sink &target, std::optional<source_location> &first)
            : m_target(target), m_first(first) {}

        void log(level lvl, const std::string &message) override
        {
            m_target.log(lvl, message);
        }

        void log(level lvl, const source_location &location, const std::string &message) override
        {
            if(lvl == level::error && !m_first)
                m_first = location;
            m_target.log(lvl, location, message);
        }

        void log(level lvl, diagnostic_code code, const source_location &location, const std::string &message) override
        {
            if(lvl == level::error && !m_first)
                m_first = location;
            m_target.log(lvl, code, location, message);
        }

    private:
        log_sink &m_target;
        std::optional<source_location> &m_first;
    };

    log_sink &m_log;
    topology_policy m_policy;
    model<double> m_model;
    bool m_ok = true;
    std::optional<source_location> m_first_failure;
};

}

#endif

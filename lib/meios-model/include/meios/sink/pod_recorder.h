#ifndef HPP_GUARD_MEIOS_MODEL_SINK_POD_RECORDER_H
#define HPP_GUARD_MEIOS_MODEL_SINK_POD_RECORDER_H

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/material.h"
#include "meios/records/robot_info.h"

#include "meios/model/topology.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/topology_policy.h"

namespace meios
{

namespace detail
{

template <typename Topology>
struct topology_scalar;

template <template <typename, typename> class Shape, typename Scalar, typename Rot>
struct topology_scalar<Shape<Scalar, Rot>>
{
    using type = Scalar;
};

}

template <typename Topology>
class pod_recorder
{
public:
    using scalar_type = typename detail::topology_scalar<Topology>::type;

    pod_recorder(log_sink &log, topology_policy policy) : m_log(log), m_policy(policy), m_result() {}

    void on_robot(const robot_info &robot)
    {
        m_result.name = robot.name;
    }

    void on_material(const material<scalar_type> &mat)
    {
        m_result.materials.push_back(mat);
    }

    void on_link(const link<scalar_type> &node)
    {
        m_result.links.push_back(node);
    }

    void on_joint(const joint<scalar_type> &edge)
    {
        m_result.joints.push_back(edge);
    }

    void finish()
    {
        m_result.parent_of =
            reconstruct_topology(m_result.links, m_result.joints, m_log, m_policy).parent_of;
    }

    const Topology &result() const
    {
        return m_result;
    }

private:
    log_sink &m_log;
    topology_policy m_policy;
    Topology m_result;
};

}

#endif

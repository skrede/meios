#ifndef HPP_GUARD_MEIOS_MODEL_SINK_POD_RECORDER_H
#define HPP_GUARD_MEIOS_MODEL_SINK_POD_RECORDER_H

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/material.h"
#include "meios/records/robot_info.h"

#include <string>
#include <cstddef>

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

template <typename Links>
int index_of(const Links &links, const std::string &name)
{
    for(std::size_t i = 0; i < links.size(); ++i)
        if(links[i].name == name)
            return static_cast<int>(i);
    return -1;
}

}

template <typename Topology>
class pod_recorder
{
public:
    using scalar_type = typename detail::topology_scalar<Topology>::type;

    pod_recorder() : m_result() {}

    void on_robot(const robot_info &robot)
    {
        m_result.name = robot.name;
    }

    void on_material(const material<scalar_type> &mat)
    {
        (void)mat;
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
        m_result.parent_of.assign(m_result.links.size(), -1);
        for(const auto &edge : m_result.joints)
            link_parent(edge);
    }

    const Topology &result() const
    {
        return m_result;
    }

private:
    void link_parent(const joint<scalar_type> &edge)
    {
        const int child = detail::index_of(m_result.links, edge.child);
        if(child >= 0)
            m_result.parent_of[static_cast<std::size_t>(child)] =
                detail::index_of(m_result.links, edge.parent);
    }

    Topology m_result;
};

}

#endif

#ifndef HPP_GUARD_MEIOS_MODEL_SINK_MODEL_SINK_H
#define HPP_GUARD_MEIOS_MODEL_SINK_MODEL_SINK_H

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/material.h"
#include "meios/records/robot_info.h"

namespace meios
{

template <typename S, typename Scalar = double>
concept model_sink = requires(
    S &sink,
    const robot_info &robot,
    const material<Scalar> &mat,
    const link<Scalar> &node,
    const joint<Scalar> &edge)
{
    { sink.on_robot(robot) };
    { sink.on_material(mat) };
    { sink.on_link(node) };
    { sink.on_joint(edge) };
    { sink.finish() };
};

}

#endif

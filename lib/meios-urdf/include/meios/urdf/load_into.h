#ifndef HPP_GUARD_MEIOS_URDF_LOAD_INTO_H
#define HPP_GUARD_MEIOS_URDF_LOAD_INTO_H

#include "meios/urdf/load.h"
#include "meios/urdf/load_result.h"

#include "meios/sink/model_sink.h"

#include "meios/model/model.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/material.h"
#include "meios/records/robot_info.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/load_error.h"
#include "meios/diagnostic/completeness.h"

#include "meios/expected.h"

#include <utility>
#include <filesystem>

namespace meios::detail
{

// A robot's extensions and version do not survive staging: the model keeps only its name.
template <typename Sink>
    requires model_sink<Sink>
void emit_model(const model<double> &staged, Sink &sink)
{
    robot_info robot;
    robot.name = staged.name;
    sink.on_robot(robot);
    for(const material<double> &mat : staged.materials)
        sink.on_material(mat);
    for(const link<double> &node : staged.links)
        sink.on_link(node);
    for(const joint<double> &edge : staged.joints)
        sink.on_joint(edge);
    sink.finish();
}

// The sink is reached only on the success arm: that ordering is the whole of the
// atomicity guarantee and the reason a consumer needs no rollback path. Moving the
// emission above the check would withdraw a published guarantee silently.
template <typename Sink>
    requires model_sink<Sink>
load_summary emit_on_success(expected<load_result, load_error> loaded, Sink &sink)
{
    if(!loaded)
        return load_summary{ std::move(loaded.error().diagnostics), completeness::none };
    emit_model(loaded->robot, sink);
    return load_summary{ std::move(loaded->diagnostics), loaded->claims };
}

}

namespace meios
{

template <typename Sink>
    requires model_sink<Sink>
load_summary load_into(const std::filesystem::path &path, const load_options &opts, Sink &sink,
                       log_sink &log)
{
    return detail::emit_on_success(load(path, opts, log), sink);
}

template <typename Sink>
    requires model_sink<Sink>
load_summary load_into(const std::filesystem::path &path, const load_options &opts, Sink &sink)
{
    return detail::emit_on_success(load(path, opts), sink);
}

}

#endif

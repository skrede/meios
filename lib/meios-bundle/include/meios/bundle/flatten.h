#ifndef HPP_GUARD_MEIOS_BUNDLE_FLATTEN_H
#define HPP_GUARD_MEIOS_BUNDLE_FLATTEN_H

#include "meios/bundle/replay.h"
#include "meios/bundle/result.h"
#include "meios/bundle/urdf_writer.h"

#include "meios/model/model.h"

#include "meios/diagnostic/log_sink.h"

#include <ostream>

namespace meios
{

// Named library entry for flattening a model to a single URDF stream, parallel to
// bundle_to_folder: it drives the shared urdf_writer with an identity rewrite so
// there is one serializer, not a second CLI-side emitter. The status/counts it
// returns drive the caller's exit code.
inline emit_result flatten(const model<double> &robot, std::ostream &out, log_sink &log)
{
    urdf_writer writer(out, log);
    replay(robot, writer);
    return writer.status();
}

}

#endif

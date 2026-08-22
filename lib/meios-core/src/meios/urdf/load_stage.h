#ifndef HPP_GUARD_MEIOS_URDF_LOAD_STAGE_H
#define HPP_GUARD_MEIOS_URDF_LOAD_STAGE_H

#include "meios/urdf/load.h"
#include "meios/urdf/load_result.h"

#include "meios/diagnostic/load_error.h"
#include "meios/diagnostic/capturing_log_sink.h"

#include "meios/expected.h"

#include <filesystem>

namespace meios
{

class source_stack;

namespace detail
{

// The one staged pass: read, sniff, expand-and-parse, then assemble whichever arm the
// diagnostics recorded during this load call for. The window is measured from the sink the
// caller's sources were built against, which is what lets a source's own failure refuse.
expected<load_result, load_error> drive_load(const std::filesystem::path &path,
                                             const load_options &opts, source_stack &sources,
                                             capture_window &window);

}

}

#endif

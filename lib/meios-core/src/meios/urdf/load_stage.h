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
// accumulated diagnostics call for. The wrapper is the caller's, because the entry point
// has to wrap before it builds the sources.
expected<load_result, load_error> drive_load(const std::filesystem::path &path,
                                             const load_options &opts, source_stack &sources,
                                             capturing_log_sink &wrapper);

}

}

#endif

#ifndef HPP_GUARD_MEIOS_SCAN_GLTF_GLB_H
#define HPP_GUARD_MEIOS_SCAN_GLTF_GLB_H

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

bool looks_like_glb(std::string_view bytes);

// Validates the 12-byte header and the first (JSON) chunk of a binary glTF and
// returns that chunk's text. The BIN chunk is self-contained and never read.
std::optional<std::string> extract_glb_json(std::string_view bytes, log_sink &log);

}

#endif

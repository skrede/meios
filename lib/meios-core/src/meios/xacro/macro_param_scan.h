#ifndef HPP_GUARD_MEIOS_XACRO_MACRO_PARAM_SCAN_H
#define HPP_GUARD_MEIOS_XACRO_MACRO_PARAM_SCAN_H

#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

// A params attribute separates its specs at every character str.isspace() holds, read whole from
// its UTF-8 encoding, so a byte inside a multi-byte character never separates.
bool breaks_at(std::string_view spec, std::size_t at);

std::size_t skip_space(std::string_view spec, std::size_t at);

std::optional<std::size_t> default_end(std::string_view spec, std::size_t at);

}

#endif

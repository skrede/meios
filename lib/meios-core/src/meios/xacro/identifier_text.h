#ifndef HPP_GUARD_MEIOS_XACRO_IDENTIFIER_TEXT_H
#define HPP_GUARD_MEIOS_XACRO_IDENTIFIER_TEXT_H

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

// The spelling Python stores for an identifier, which is its NFKC form, or nothing when the text
// is not an identifier.
std::optional<std::string> python_name(std::string_view spelling);

// The reference's is_valid_name: the name is a Python identifier, its own NFKC form, and no hard
// keyword.
bool valid_python_name(std::string_view name);

}

#endif

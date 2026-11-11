#ifndef HPP_GUARD_MEIOS_XACRO_PYTHON_SPACE_H
#define HPP_GUARD_MEIOS_XACRO_PYTHON_SPACE_H

#include <cstddef>
#include <string_view>

namespace meios::detail
{

// Python 3.14's str.strip(): the text without the characters str.isspace() holds at either end.
std::string_view python_stripped(std::string_view text);

// The byte length of the character starting at `at` when str.isspace() holds it, otherwise 0.
std::size_t python_space_at(std::string_view text, std::size_t at);

}

#endif

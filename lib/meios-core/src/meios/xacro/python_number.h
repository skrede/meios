#ifndef HPP_GUARD_MEIOS_XACRO_PYTHON_NUMBER_H
#define HPP_GUARD_MEIOS_XACRO_PYTHON_NUMBER_H

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

// The text grammars of Python 3.14's int() and float(), docs.python.org/3/library/functions.html:
// Unicode decimal digits, single underscores between digits, a sign and surrounding whitespace.

// The canonical decimal text of what int() reads, without leading zeros and with -0 as 0, or
// nothing where int() raises.
std::optional<std::string> python_int(std::string_view text);

// What float() reads, the infinities and nan included, or nothing where float() raises.
std::optional<double> python_real(std::string_view text);

}

#endif

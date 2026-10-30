#ifndef HPP_GUARD_MEIOS_XACRO_BOOLEAN_VALUE_H
#define HPP_GUARD_MEIOS_XACRO_BOOLEAN_VALUE_H

#include "meios/xacro/value.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

// The canonical decimal text of what Python's int() reads from the text, or nothing where int()
// raises. Python also reads Unicode digits and Unicode whitespace there; only ASCII is read here.
std::optional<std::string> python_int(std::string_view text);

// The reference's get_boolean_value (xacro 2.1.1): a string reads true, True, false and False by
// spelling and any other text through int(), and a typed value by its Python truth.
std::optional<bool> get_boolean_value(const value &v);

}

#endif

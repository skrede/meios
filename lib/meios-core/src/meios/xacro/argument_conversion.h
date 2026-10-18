#ifndef HPP_GUARD_MEIOS_XACRO_ARGUMENT_CONVERSION_H
#define HPP_GUARD_MEIOS_XACRO_ARGUMENT_CONVERSION_H

#include "meios/xacro/value.h"

#include <optional>
#include <string_view>

namespace meios::detail
{

// Upstream's convert_value(text, 'auto') (xacro 2.1.1, substitution_args): a real where the text
// holds a '.', an integer otherwise, then the two boolean spellings in any letter case, then the
// text itself, with digits and spaces read as Python 3.14's int() and float() read them. A number
// with no value here answers nothing rather than becoming text.
std::optional<value> converted_argument(std::string_view text);

}

#endif

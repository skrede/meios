#ifndef HPP_GUARD_MEIOS_XACRO_BOOLEAN_VALUE_H
#define HPP_GUARD_MEIOS_XACRO_BOOLEAN_VALUE_H

#include "meios/xacro/value.h"

#include <optional>

namespace meios::detail
{

// The one boolean reading of a condition or a boolean attribute. Text reads by the spellings true,
// True, false and False, then as an integer or a finite real by whether it is nonzero; any other
// text is no boolean. A typed value reads by its truth, a collection true when it is non-empty.
std::optional<bool> get_boolean_value(const value &v);

}

#endif

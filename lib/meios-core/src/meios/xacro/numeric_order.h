#ifndef HPP_GUARD_MEIOS_XACRO_NUMERIC_ORDER_H
#define HPP_GUARD_MEIOS_XACRO_NUMERIC_ORDER_H

#include "meios/xacro/value.h"

#include <compare>
#include <optional>

namespace meios::detail
{

// A boolean, an integer of any width and a real are numbers; a boolean reads as 0 or 1.
bool is_number(const value &v);

// The exact order of two numbers, at full precision on both sides, and nothing when either
// operand is not a number.
std::optional<std::partial_ordering> numeric_order(const value &a, const value &b);

}

#endif

#include "numeric_order.h"
#include "numeric_detail.h"

#include "meios/xacro/value.h"

#include "meios/xacro/detail/wide_integer.h"

#include <cmath>
#include <string>
#include <compare>
#include <cstdint>
#include <variant>
#include <optional>
#include <string_view>

namespace meios::detail
{
namespace
{

using number = std::variant<std::int64_t, double, wide_integer>;

constexpr double integer_span = 9223372036854775808.0;

std::optional<number> as_number(const value &v)
{
    if(std::optional<bool> flag = v.boolean())
        return number{ std::int64_t{ *flag ? 1 : 0 } };
    if(std::optional<std::int64_t> integer = v.integer())
        return number{ *integer };
    if(std::optional<double> real = v.real())
        return number{ *real };
    if(std::optional<wide_integer> wide = v.wide())
        return number{ *wide };
    return std::nullopt;
}

// Canonical decimal spellings order by sign, then digit count, then digits; between two negative
// spellings the larger magnitude is the smaller number.
std::partial_ordering order_spellings(std::string_view a, std::string_view b)
{
    const bool negative = a.starts_with('-');
    if(negative != b.starts_with('-'))
        return negative ? std::partial_ordering::less : std::partial_ordering::greater;
    std::strong_ordering magnitude = a.size() <=> b.size();
    if(magnitude == 0)
        magnitude = a <=> b;
    return negative ? 0 <=> magnitude : magnitude;
}

std::partial_ordering order(std::int64_t a, std::int64_t b)
{
    return a <=> b;
}

std::partial_ordering order(double a, double b)
{
    return a <=> b;
}

// Exact over the whole 64-bit range, as integer_equals_real in value_key.h is: a real at or past
// 2^63 in magnitude decides by its sign, any other by its truncation and then its fraction.
std::partial_ordering order(std::int64_t a, double b)
{
    if(b >= integer_span)
        return std::partial_ordering::less;
    if(b < -integer_span)
        return std::partial_ordering::greater;
    const double whole = std::trunc(b);
    const std::int64_t truncated = static_cast<std::int64_t>(whole);
    if(a != truncated)
        return a <=> truncated;
    return 0.0 <=> (b - whole);
}

std::partial_ordering order(const wide_integer &a, std::int64_t)
{
    return a.negative() ? std::partial_ordering::less : std::partial_ordering::greater;
}

std::partial_ordering order(const wide_integer &a, const wide_integer &b)
{
    return order_spellings(a.spelling(), b.spelling());
}

// A real at or past 2^63 in magnitude is a whole number, so its exact digits order it against
// the wide integer without rounding either side.
std::partial_ordering order(const wide_integer &a, double b)
{
    if(b > -integer_span && b < integer_span)
        return a.negative() ? std::partial_ordering::less : std::partial_ordering::greater;
    return order_spellings(a.spelling(), integral_digits(b));
}

std::partial_ordering order(double a, std::int64_t b)
{
    return 0 <=> order(b, a);
}

std::partial_ordering order(std::int64_t a, const wide_integer &b)
{
    return 0 <=> order(b, a);
}

std::partial_ordering order(double a, const wide_integer &b)
{
    return 0 <=> order(b, a);
}

}

bool is_number(const value &v)
{
    return as_number(v).has_value();
}

std::optional<std::partial_ordering> numeric_order(const value &a, const value &b)
{
    const std::optional<number> left = as_number(a);
    const std::optional<number> right = as_number(b);
    if(!left || !right)
        return std::nullopt;
    return std::visit([](const auto &x, const auto &y) -> std::partial_ordering { return order(x, y); },
                      *left, *right);
}

}

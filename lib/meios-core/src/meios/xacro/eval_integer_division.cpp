#include "big_integer.h"
#include "eval_parser.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"

#include <string>
#include <cstdint>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

constexpr std::int64_t exact_real_bound = std::int64_t{ 1 } << 53;

bool is_zero(const value &v)
{
    return !v.wide() && *as_int(v) == 0;
}

// Every integer up to 2^53 in magnitude is a real exactly, so dividing two such reals rounds the
// quotient once, as dividing the integers does.
bool exact_as_reals(std::optional<std::int64_t> number)
{
    return number && *number >= -exact_real_bound && *number <= exact_real_bound;
}

value refuse_zero(parser &p, std::string_view op)
{
    return p.fail("the divisor of '" + std::string(op) + "' is zero");
}

}

value integer_floor_divide(parser &p, const value &a, const value &b)
{
    if(is_zero(b))
        return refuse_zero(p, "//");
    const std::optional<std::int64_t> x = as_int(a), y = as_int(b);
    if(x && y)
        if(const std::optional<std::int64_t> made = checked_floordiv(*x, *y))
            return value{ *made };
    return integer_result(p, floor_divmod(big_integer_of(a), big_integer_of(b)).first, "floor division");
}

value integer_modulo(parser &p, const value &a, const value &b)
{
    if(is_zero(b))
        return refuse_zero(p, "%");
    const std::optional<std::int64_t> x = as_int(a), y = as_int(b);
    if(x && y)
        if(const std::optional<std::int64_t> made = checked_mod(*x, *y))
            return value{ *made };
    return integer_result(p, floor_divmod(big_integer_of(a), big_integer_of(b)).second, "remainder");
}

value integer_divide(parser &p, const value &a, const value &b)
{
    if(is_zero(b))
        return refuse_zero(p, "/");
    const std::optional<std::int64_t> x = as_int(a), y = as_int(b);
    if(exact_as_reals(x) && exact_as_reals(y))
        return real_result(p, static_cast<double>(*x) / static_cast<double>(*y));
    const std::optional<double> quotient = true_quotient(big_integer_of(a), big_integer_of(b));
    if(!quotient)
        return p.fail("the quotient of '/' is too large for a real");
    return real_result(p, *quotient);
}

}

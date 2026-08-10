#include "lexer.h"
#include "eval_parser.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"
#include "meios/xacro/value_render.h"
#include "meios/xacro/detail/numeric.h"

#include <cmath>
#include <string>
#include <cstdint>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

// A string does have an arithmetic and a truth meaning in Python — concatenation,
// repetition, emptiness — and none of it is in the measured surface, so it refuses as
// unsupported and a lenient policy may still retain the span. Every other kind reaching
// here has no meaning upstream either, which makes it a genuine fault.
value refuse_kind(parser &p, const value &v)
{
    const std::string message =
        "a " + std::string(kind_name(v.kind())) + " has no arithmetic meaning here";
    if(v.kind() == value_kind::string)
        return p.fail_unsupported(message + " — use eval-python");
    return p.fail(message);
}

}

value int_result(parser &p, std::optional<std::int64_t> made, std::string_view operation)
{
    if(made)
        return value{ *made };
    return p.fail(std::string(operation)
                  + " produced a result outside the range of integers this evaluator represents");
}

value int_from_real(parser &p, double number)
{
    const std::optional<std::int64_t> made = int_from_double(number);
    if(made)
        return value{ *made };
    return p.fail(print_double(number)
                  + " is outside the range of integers this evaluator represents");
}

std::int64_t need_int(parser &p, const value &v)
{
    std::optional<std::int64_t> number = as_int(v);
    if(number)
        return *number;
    if(v.kind() == value_kind::real)
        int_from_real(p, *v.real());
    else
        refuse_kind(p, v);
    return 0;
}

double need_double(parser &p, const value &v)
{
    std::optional<double> number = as_double(v);
    if(number)
        return *number;
    refuse_kind(p, v);
    return 0.0;
}

bool need_truth(parser &p, const value &v)
{
    std::optional<bool> truth = truthy(v);
    if(truth)
        return *truth;
    refuse_kind(p, v);
    return false;
}

// The public contract admits finite floating-point only, so an overflowing or undefined
// arithmetic result fails here rather than reaching a document as inf or nan.
value real_result(parser &p, double number)
{
    std::optional<value> made = value::make_real(number);
    if(made)
        return *made;
    return p.fail("the expression result is not a finite number");
}

value negate(parser &p, const value &v)
{
    if(v.kind() == value_kind::real)
        return real_result(p, -*v.real());
    return int_result(p, checked_negate(need_int(p, v)), "negation");
}

value unary_pos(parser &p, const value &v)
{
    if(v.kind() == value_kind::real)
        return v;
    return value{ need_int(p, v) };
}

value add_sub_mul(parser &p, const value &a, const value &b, token_kind op)
{
    if(is_int(a) && is_int(b))
    {
        std::int64_t x = need_int(p, a), y = need_int(p, b);
        if(op == token_kind::plus)  return int_result(p, checked_add(x, y), "addition");
        if(op == token_kind::minus) return int_result(p, checked_sub(x, y), "subtraction");
        return int_result(p, checked_mul(x, y), "multiplication");
    }
    double x = need_double(p, a), y = need_double(p, b);
    if(op == token_kind::plus)  return real_result(p, x + y);
    if(op == token_kind::minus) return real_result(p, x - y);
    return real_result(p, x * y);
}

value divide(parser &p, const value &a, const value &b)
{
    double y = need_double(p, b);
    if(y == 0.0) return p.fail("division by zero");
    return real_result(p, need_double(p, a) / y);
}

value floor_divide(parser &p, const value &a, const value &b)
{
    if(is_int(a) && is_int(b))
    {
        if(need_int(p, b) == 0) return p.fail("integer division by zero");
        return int_result(p, checked_floordiv(need_int(p, a), need_int(p, b)), "floor division");
    }
    double y = need_double(p, b);
    if(y == 0.0) return p.fail("float floor division by zero");
    return real_result(p, std::floor(need_double(p, a) / y));
}

value modulo(parser &p, const value &a, const value &b)
{
    if(is_int(a) && is_int(b))
    {
        if(need_int(p, b) == 0) return p.fail("integer modulo by zero");
        return int_result(p, checked_mod(need_int(p, a), need_int(p, b)), "remainder");
    }
    double y = need_double(p, b);
    if(y == 0.0) return p.fail("float modulo by zero");
    double r = std::fmod(need_double(p, a), y);
    if(r != 0.0 && ((r < 0.0) != (y < 0.0))) r += y;
    return real_result(p, r);
}

value power(parser &p, const value &a, const value &b)
{
    if(is_int(a) && is_int(b) && need_int(p, b) >= 0)
    {
        const std::optional<std::int64_t> made = checked_power(need_int(p, a), need_int(p, b));
        if(!made) return p.fail("integer power overflow");
        return value{ *made };
    }
    return real_result(p, std::pow(need_double(p, a), need_double(p, b)));
}

}

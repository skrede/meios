#include "lexer.h"
#include "eval_parser.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"
#include "meios/xacro/value_render.h"

#include <cmath>
#include <limits>
#include <string>
#include <cstdint>
#include <optional>

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

std::int64_t floordiv_int(std::int64_t a, std::int64_t b)
{
    std::int64_t q = a / b, r = a % b;
    if(r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

std::int64_t mod_int(std::int64_t a, std::int64_t b)
{
    std::int64_t r = a % b;
    if(r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

bool mul_overflows(std::int64_t a, std::int64_t b, std::int64_t &out)
{
    constexpr std::int64_t lo = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t hi = std::numeric_limits<std::int64_t>::max();
    if(a > 0)
    {
        if(b > 0) { if(a > hi / b) return true; }
        else      { if(b < lo / a) return true; }
    }
    else if(a < 0)
    {
        if(b > 0) { if(a < lo / b) return true; }
        else      { if(b < hi / a) return true; }
    }
    out = a * b;
    return false;
}

}

std::int64_t need_int(parser &p, const value &v)
{
    std::optional<std::int64_t> number = as_int(v);
    if(number)
        return *number;
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
    return value{ -need_int(p, v) };
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
        if(op == token_kind::plus)  return value{ x + y };
        if(op == token_kind::minus) return value{ x - y };
        return value{ x * y };
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
        return value{ floordiv_int(need_int(p, a), need_int(p, b)) };
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
        return value{ mod_int(need_int(p, a), need_int(p, b)) };
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
        std::int64_t base = need_int(p, a), exp = need_int(p, b), result = 1;
        while(exp > 0)
        {
            if((exp & 1) && mul_overflows(result, base, result)) return p.fail("integer power overflow");
            exp >>= 1;
            if(exp > 0 && mul_overflows(base, base, base)) return p.fail("integer power overflow");
        }
        return value{ result };
    }
    return real_result(p, std::pow(need_double(p, a), need_double(p, b)));
}

}

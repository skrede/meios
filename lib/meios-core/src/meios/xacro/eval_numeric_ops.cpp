#include "lexer.h"
#include "eval_parser.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"
#include "meios/xacro/value_render.h"

#include "meios/xacro/detail/wide_integer.h"

#include "meios/detail/article.h"

#include <cmath>
#include <string>
#include <cstdint>
#include <optional>

namespace meios::detail
{

namespace
{

// Concatenation is the one arithmetic meaning a string carries here; repetition by an integer is
// the meaning that stays outside the measured surface, so a string reaching any other operator
// refuses as unsupported and a lenient policy may still retain the span. Every other kind reaching
// here has no arithmetic meaning in Python, which makes it a genuine fault.
value refuse_kind(parser &p, const value &v)
{
    const std::string message = with_article(kind_name(v.kind())) + " operand has no arithmetic meaning here";
    if(v.kind() == value_kind::string)
        return p.fail_unsupported(message + " — use eval-python");
    return p.fail(message);
}

// Two strings concatenate; a string beside any other kind is a type mismatch, reported by the kind
// that is not the string.
value concatenate(parser &p, const value &a, const value &b)
{
    const std::optional<std::string> left = a.text();
    const std::optional<std::string> right = b.text();
    if(left && right)
    {
        p.exercised(evaluator_construct::string_concatenation);
        return text_result(p, *left + *right);
    }
    return p.fail_unsupported("a string concatenates only with another string operand, not with " + with_article(kind_name(left ? b.kind() : a.kind())) + " — use eval-python");
}

// Counts the divisor from the exact remainder so the quotient never rounds up across a whole
// number, after CPython's float_divmod (Objects/floatobject.c).
double floored_quotient(double x, double y)
{
    const double remainder = std::fmod(x, y);
    double quotient = (x - remainder) / y;
    if(remainder != 0.0 && ((remainder < 0.0) != (y < 0.0)))
        quotient -= 1.0;
    if(quotient == 0.0)
        return std::copysign(0.0, x / y);
    const double floored = std::floor(quotient);
    if(quotient - floored > 0.5)
        return floored + 1.0;
    return floored;
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
    if(!admits_digits(p, v))
        return 0.0;
    std::optional<double> number = as_double(v);
    if(number)
        return *number;
    if(const std::optional<wide_integer> wide = v.wide())
        p.fail("an integer of " + std::to_string(wide->spelling().size() - (wide->negative() ? 1 : 0)) + " digits is too large for a real");
    else
        refuse_kind(p, v);
    return 0.0;
}

bool need_truth(parser &p, const value &v)
{
    std::optional<bool> truth = truthy(v);
    if(!truth)
    {
        refuse_kind(p, v);
        return false;
    }
    if(v.kind() == value_kind::string)
        p.exercised(evaluator_construct::string_truth);
    return *truth;
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
    if(is_integral(v))
        return integer_negate(p, v);
    return value{ need_int(p, v) };
}

value unary_pos(parser &p, const value &v)
{
    if(v.kind() == value_kind::real || v.kind() == value_kind::wide_integer)
        return v;
    return value{ need_int(p, v) };
}

value add_sub_mul(parser &p, const value &a, const value &b, token_kind op)
{
    if(op == token_kind::plus
       && (a.kind() == value_kind::string || b.kind() == value_kind::string))
        return concatenate(p, a, b);
    if(is_integral(a) && is_integral(b))
        return integer_arithmetic(p, a, b, op);
    double x = need_double(p, a), y = need_double(p, b);
    if(op == token_kind::plus)  return real_result(p, x + y);
    if(op == token_kind::minus) return real_result(p, x - y);
    return real_result(p, x * y);
}

value divide(parser &p, const value &a, const value &b)
{
    if(is_integral(a) && is_integral(b))
        return integer_divide(p, a, b);
    double y = need_double(p, b);
    if(y == 0.0)
        return p.fail("the divisor of '/' is zero");
    return real_result(p, need_double(p, a) / y);
}

value floor_divide(parser &p, const value &a, const value &b)
{
    if(is_integral(a) && is_integral(b))
        return integer_floor_divide(p, a, b);
    double y = need_double(p, b);
    if(y == 0.0)
        return p.fail("the divisor of '//' is zero");
    return real_result(p, floored_quotient(need_double(p, a), y));
}

value modulo(parser &p, const value &a, const value &b)
{
    if(is_integral(a) && is_integral(b))
        return integer_modulo(p, a, b);
    double y = need_double(p, b);
    if(y == 0.0)
        return p.fail("the divisor of '%' is zero");
    double r = std::fmod(need_double(p, a), y);
    if(r != 0.0 && ((r < 0.0) != (y < 0.0))) r += y;
    // A zero remainder takes the divisor's sign, after CPython's float_rem (Objects/floatobject.c).
    if(r == 0.0) r = std::copysign(0.0, y);
    return real_result(p, r);
}

value power(parser &p, const value &a, const value &b)
{
    if(integral_power(a, b))
        return integer_power(p, a, b);
    if(!admits_power(p, a, b))
        return value{};
    return real_result(p, std::pow(need_double(p, a), need_double(p, b)));
}

}

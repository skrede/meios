#include "lexer.h"
#include "big_integer.h"
#include "eval_parser.h"
#include "eval_session.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"

#include "meios/xacro/detail/wide_integer.h"

#include <cmath>
#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <charconv>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

std::optional<std::int64_t> checked(std::int64_t a, std::int64_t b, token_kind op)
{
    if(op == token_kind::plus)
        return checked_add(a, b);
    if(op == token_kind::minus)
        return checked_sub(a, b);
    return checked_mul(a, b);
}

value refuse_digits(parser &p, std::string_view operation)
{
    return p.fail(std::string(operation) + " produced an integer of more than the " + std::to_string(wide_digit_ceiling)
                  + " digits an integer holds here");
}

// The leading fifteen digits carry the logarithm to the precision a double holds; the rest only
// shift it by their count.
double log10_magnitude(const value &v)
{
    const std::optional<wide_integer> wide = v.wide();
    if(!wide)
        return std::log10(std::fabs(static_cast<double>(*as_int(v))));
    const std::string_view digits = std::string_view(wide->spelling()).substr(wide->negative() ? 1 : 0);
    const std::size_t leading = std::min<std::size_t>(digits.size(), 15);
    std::uint64_t head = 0;
    std::from_chars(digits.data(), digits.data() + leading, head);
    return std::log10(static_cast<double>(head)) + static_cast<double>(digits.size() - leading);
}

// The estimate is the exact digit count or one more, so only an estimate past the ceiling by more
// than one digit is certain to be past it; the one digit left is caught on the computed result.
bool certainly_too_long(const value &base, std::int64_t exponent)
{
    const double digits = std::ceil(static_cast<double>(exponent) * log10_magnitude(base)) + 1.0;
    return digits > static_cast<double>(wide_digit_ceiling + 1);
}

// A base of -1, 0 or 1 has a closed form for any exponent, so a wide exponent costs it nothing.
value closed_power(const big_integer &base, const wide_integer &exponent)
{
    const bool odd = (exponent.spelling().back() - '0') % 2 == 1;
    if(base.negative() && odd)
        return value{ std::int64_t{ -1 } };
    return value{ std::int64_t{ base.bit_length() == 0 ? 0 : 1 } };
}

}

big_integer big_integer_of(const value &v)
{
    if(const std::optional<wide_integer> wide = v.wide())
        return *big_integer::from_spelling(wide->spelling());
    return big_integer{ *as_int(v) };
}

bool is_integral(const value &v)
{
    return is_int(v) || v.kind() == value_kind::wide_integer;
}

value integer_result(parser &p, const big_integer &number, std::string_view operation)
{
    if(const std::optional<std::int64_t> narrow = number.int64())
        return value{ *narrow };
    std::optional<wide_integer> wide = wide_integer::from_spelling(number.spelling());
    if(!wide)
        return refuse_digits(p, operation);
    const std::size_t digits = wide->spelling().size() - (wide->negative() ? 1 : 0);
    if(!p.session.admits_string_length(digits, p.log, p.anchor) || !p.session.charge_bytes(digits, p.log, p.anchor))
    {
        p.refuse_exhausted();
        return value{};
    }
    return value{ std::move(*wide) };
}

value integer_arithmetic(parser &p, const value &a, const value &b, token_kind op)
{
    const std::optional<std::int64_t> x = as_int(a), y = as_int(b);
    if(x && y)
        if(const std::optional<std::int64_t> made = checked(*x, *y, op))
            return value{ *made };
    if(op == token_kind::plus)
        return integer_result(p, big_integer_of(a) + big_integer_of(b), "addition");
    if(op == token_kind::minus)
        return integer_result(p, big_integer_of(a) - big_integer_of(b), "subtraction");
    return integer_result(p, big_integer_of(a) * big_integer_of(b), "multiplication");
}

value integer_negate(parser &p, const value &v)
{
    if(const std::optional<std::int64_t> x = as_int(v))
        if(const std::optional<std::int64_t> made = checked_negate(*x))
            return value{ *made };
    return integer_result(p, -big_integer_of(v), "negation");
}

value integer_abs(parser &p, const value &v)
{
    if(const std::optional<std::int64_t> x = as_int(v))
        if(const std::optional<std::int64_t> made = checked_abs(*x))
            return value{ *made };
    return integer_result(p, abs(big_integer_of(v)), "absolute value");
}

bool integral_power(const value &base, const value &exponent)
{
    if(!is_integral(base) || !is_integral(exponent))
        return false;
    const std::optional<wide_integer> wide = exponent.wide();
    return wide ? !wide->negative() : *as_int(exponent) >= 0;
}

value integer_power(parser &p, const value &a, const value &b)
{
    const std::optional<std::int64_t> base = as_int(a), exponent = as_int(b);
    if(base && exponent)
        if(const std::optional<std::int64_t> made = checked_power(*base, *exponent))
            return value{ *made };
    const big_integer exact_base = big_integer_of(a);
    if(!exponent && exact_base.bit_length() <= 1)
        return closed_power(exact_base, *b.wide());
    if(!exponent || certainly_too_long(a, *exponent))
        return refuse_digits(p, "exponentiation");
    return integer_result(p, pow(exact_base, static_cast<std::uint64_t>(*exponent)), "exponentiation");
}

}

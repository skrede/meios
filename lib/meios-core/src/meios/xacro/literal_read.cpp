#include "literal_read.h"
#include "python_number.h"

#include "meios/xacro/value.h"
#include "meios/xacro/value_kind.h"

#include "meios/xacro/detail/numeric.h"
#include "meios/xacro/detail/wide_integer.h"

#include <string>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

bool quoted(std::string_view text)
{
    return text.size() >= 2 && text.front() == '\'' && text.back() == '\'';
}

value integer(const std::string &digits)
{
    bool ok = false;
    const std::int64_t number = parse_int(digits, ok);
    return ok ? value{ number } : value{ digits };
}

std::optional<std::string> canonical_integer(std::string_view digits)
{
    const bool negative = digits.starts_with('-');
    std::string_view magnitude = negative ? digits.substr(1) : digits;
    if(magnitude.empty() || magnitude.find_first_not_of("0123456789") != std::string_view::npos)
        return std::nullopt;
    magnitude = magnitude.substr(std::min(magnitude.find_first_not_of('0'), magnitude.size() - 1));
    return (negative && magnitude != "0" ? "-" : "") + std::string(magnitude);
}

}

std::optional<value> integer_literal(std::string_view digits)
{
    const std::optional<std::string> spelling = canonical_integer(digits);
    if(!spelling)
        return std::nullopt;
    bool ok = false;
    const std::int64_t number = parse_int(*spelling, ok);
    if(ok)
        return value{ number };
    const std::optional<wide_integer> wide = wide_integer::from_spelling(*spelling);
    return wide ? std::optional<value>(value{ *wide }) : std::nullopt;
}

value read_literal(std::string_view text)
{
    if(quoted(text))
        return value{ std::string(text.substr(1, text.size() - 2)) };
    if(text.find('_') != std::string_view::npos)
        return value{ std::string(text) };
    if(const std::optional<std::string> digits = python_int(text))
        return integer(*digits);
    if(const std::optional<double> real = python_real(text))
        if(std::optional<value> number = value::make_real(*real))
            return *number;
    if(text == "true" || text == "True")
        return value{ true };
    if(text == "false" || text == "False")
        return value{ false };
    return value{ std::string(text) };
}

value read_literal(const value &bound)
{
    return bound.kind() == value_kind::string ? read_literal(*bound.text()) : bound;
}

}

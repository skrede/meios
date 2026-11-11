#include "literal_read.h"
#include "python_number.h"

#include "meios/xacro/value.h"
#include "meios/xacro/value_kind.h"

#include "meios/xacro/detail/numeric.h"

#include <string>
#include <cstdint>
#include <optional>
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

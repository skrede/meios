#include "boolean_value.h"
#include "python_number.h"

#include <cmath>
#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

std::optional<bool> spelled_boolean(std::string_view text)
{
    if(text == "true" || text == "True")
        return true;
    if(text == "false" || text == "False")
        return false;
    const std::optional<std::string> integer = python_int(text);
    if(integer)
        return *integer != "0";
    const std::optional<double> real = python_real(text);
    if(!real || !std::isfinite(*real))
        return std::nullopt;
    return *real != 0.0;
}

}

std::optional<bool> get_boolean_value(const value &v)
{
    switch(v.kind())
    {
        case value_kind::null:     return false;
        case value_kind::boolean:  return *v.boolean();
        case value_kind::integer:  return *v.integer() != 0;
        case value_kind::real:     return *v.real() != 0.0;
        case value_kind::string:   return spelled_boolean(*v.text());
        case value_kind::sequence:
        case value_kind::mapping:  return v.size() != 0;
    }
    return std::nullopt;
}

}

#ifndef HPP_GUARD_MEIOS_XACRO_VALUE_H
#define HPP_GUARD_MEIOS_XACRO_VALUE_H

#include "meios/xacro/detail/numeric.h"

#include <string>
#include <variant>

namespace meios
{

using value = std::variant<long long, double, bool>;

inline std::string to_python_str(const value &v)
{
    if(std::holds_alternative<bool>(v))
        return std::get<bool>(v) ? "True" : "False";
    if(std::holds_alternative<long long>(v))
        return std::to_string(std::get<long long>(v));
    return detail::print_double(std::get<double>(v));
}

}

#endif

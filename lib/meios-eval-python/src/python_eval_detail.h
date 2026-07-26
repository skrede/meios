#ifndef HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_DETAIL_H
#define HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_DETAIL_H

#include "guard_prelude.h"

#include "meios/xacro/eval_scope.h"

#include <pybind11/embed.h>

#include <cctype>
#include <string>
#include <vector>
#include <cstddef>
#include <string_view>

namespace meios::detail
{

inline std::vector<std::string> identifiers(std::string_view expr)
{
    std::vector<std::string> names;
    std::size_t i = 0;
    while(i < expr.size())
    {
        if(!std::isalpha(static_cast<unsigned char>(expr[i])) && expr[i] != '_')
        {
            ++i;
            continue;
        }
        std::size_t start = i;
        while(i < expr.size() && (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '_'))
            ++i;
        names.emplace_back(expr.substr(start, i - start));
    }
    return names;
}

inline pybind11::dict privileged_namespace()
{
    pybind11::dict priv;
    pybind11::exec(std::string(guard_prelude_source()), priv);
    return priv;
}

inline pybind11::set bound_names(std::string_view expr, const eval_scope &scope)
{
    pybind11::set bound;
    for(const std::string &name : identifiers(expr))
    {
        if(scope.lookup(name))
            bound.add(pybind11::str(name));
    }
    return bound;
}

inline std::string refusal_rule(const pybind11::dict &priv, std::string_view expr, const pybind11::set &bound)
{
    pybind11::object rule = priv["check_expression"](pybind11::str(std::string(expr)), bound);
    return rule.cast<std::string>();
}

}

#endif

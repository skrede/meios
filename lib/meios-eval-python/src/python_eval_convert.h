#ifndef HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_CONVERT_H
#define HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_CONVERT_H

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/container_marker.h"

#include <pybind11/embed.h>

#include <string>
#include <variant>

namespace meios::detail
{

// Only a marker-wrapped string is re-parsed with ast.literal_eval and rebound to its real
// container; any other string is returned verbatim, so a literal-shaped author value like
// "[1, 2]" stays a string, matching xacro's _eval_literal.
inline pybind11::object rehydrate(const std::string &text)
{
    if(text.size() < 2 || text.front() != container_marker || text.back() != container_marker)
        return pybind11::str(text);
    const std::string inner = text.substr(1, text.size() - 2);
    try
    {
        pybind11::object parsed = pybind11::module_::import("ast").attr("literal_eval")(inner);
        if(pybind11::isinstance<pybind11::dict>(parsed) || pybind11::isinstance<pybind11::list>(parsed)
            || pybind11::isinstance<pybind11::tuple>(parsed))
            return parsed;
    }
    catch(pybind11::error_already_set &)
    {
    }
    return pybind11::str(inner);
}

inline pybind11::object to_py_object(const binding &bound)
{
    if(std::holds_alternative<std::string>(bound))
        return rehydrate(std::get<std::string>(bound));
    const value &v = std::get<value>(bound);
    if(std::holds_alternative<bool>(v))
        return pybind11::bool_(std::get<bool>(v));
    if(std::holds_alternative<long long>(v))
        return pybind11::int_(std::get<long long>(v));
    return pybind11::float_(std::get<double>(v));
}

// bool/float route back through the core evaluator's to_python_str so the two paths render
// identically (True/False, .0-append); str returns verbatim; int renders via Python's own
// str() to preserve arbitrary-precision results (${2**64}) that overflow long long, bool
// preceding int because Python bool subtypes int. A list/dict/tuple is delimited with the
// container_marker so rehydrate can round-trip it across a xacro:property boundary.
inline std::string format_result(const pybind11::object &result)
{
    if(pybind11::isinstance<pybind11::str>(result))
        return result.cast<std::string>();
    if(pybind11::isinstance<pybind11::bool_>(result))
        return to_python_str(value{ result.cast<bool>() });
    if(pybind11::isinstance<pybind11::float_>(result))
        return to_python_str(value{ result.cast<double>() });
    if(pybind11::isinstance<pybind11::list>(result) || pybind11::isinstance<pybind11::dict>(result)
        || pybind11::isinstance<pybind11::tuple>(result))
        return container_marker + pybind11::str(result).cast<std::string>() + container_marker;
    return pybind11::str(result).cast<std::string>();
}

}

#endif

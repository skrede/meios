#ifndef HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_CONVERT_H
#define HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_CONVERT_H

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/value_render.h"
#include "meios/xacro/container_marker.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pybind11/embed.h>

#include <string>
#include <cstdint>
#include <utility>
#include <optional>
#include <algorithm>
#include <stdexcept>

namespace meios::detail
{

inline bool delimits_container(char c)
{
    return std::find(container_markers.begin(), container_markers.end(), c) != container_markers.end();
}

inline pybind11::object wrap_yaml(const pybind11::dict &priv, pybind11::object plain)
{
    if(pybind11::isinstance<pybind11::dict>(plain))
        return priv["_YamlDict"](std::move(plain));
    if(pybind11::isinstance<pybind11::list>(plain))
        return priv["_YamlList"](std::move(plain));
    return plain;
}

inline bool is_yaml_wrapper(const pybind11::dict &priv, const pybind11::object &obj)
{
    const pybind11::object mapping  = priv["_YamlDict"];
    const pybind11::object sequence = priv["_YamlList"];
    return pybind11::isinstance(obj, mapping) || pybind11::isinstance(obj, sequence);
}

// Only a marker-wrapped string is re-parsed with ast.literal_eval and rebound to its real
// container; any other string is returned verbatim, so a literal-shaped author value like
// "[1, 2]" stays a string, matching xacro's _eval_literal. Which marker delimited the text is
// what selects a wrapped reconstruction over a plain one.
inline pybind11::object rehydrate(const pybind11::dict &priv, const std::string &text)
{
    if(text.size() < 2 || text.front() != text.back() || !delimits_container(text.front()))
        return pybind11::str(text);
    const std::string inner = text.substr(1, text.size() - 2);
    try
    {
        pybind11::object parsed = pybind11::module_::import("ast").attr("literal_eval")(inner);
        if(pybind11::isinstance<pybind11::dict>(parsed) || pybind11::isinstance<pybind11::list>(parsed)
            || pybind11::isinstance<pybind11::tuple>(parsed))
            return text.front() == yaml_container_marker ? wrap_yaml(priv, parsed) : parsed;
    }
    catch(pybind11::error_already_set &)
    {
    }
    return pybind11::str(inner);
}

inline pybind11::object to_py_object(const pybind11::dict &priv, const value &bound)
{
    if(std::optional<std::string> text = bound.text())
        return rehydrate(priv, *text);
    if(std::optional<bool> flag = bound.boolean())
        return pybind11::bool_(*flag);
    if(std::optional<std::int64_t> number = bound.integer())
        return pybind11::int_(*number);
    if(std::optional<double> number = bound.real())
        return pybind11::float_(*number);
    return pybind11::none();
}

// bool/float route back through the core evaluator's scalar renderer so the two paths render
// identically (True/False, .0-append); str returns verbatim; int renders via Python's own
// str() to preserve arbitrary-precision results (${2**64}) that overflow an int64_t, bool
// preceding int because Python bool subtypes int. A list/dict/tuple is delimited with a marker
// so rehydrate can round-trip it across a xacro:property boundary. The yaml-wrapper test must
// precede the plain one: PyDict_Check and PyList_Check match subclasses, so the plain branch
// would otherwise stamp every wrapper with the plain marker and provenance would die there.
// The chain is exhaustive and anything past its end is refused by type name: the tail used to
// render whatever str() produced, which wrote live heap addresses and whole loaded
// configurations into documents consumers commit, share and diff. A null is the one member
// admitted rather than refused, because a yaml key written with no value parses to one and the
// compatibility target flattens it as the text None.
inline std::string format_result(const pybind11::dict &priv, log_sink &log, const pybind11::object &result)
{
    if(result.is_none())
    {
        log.log(level::warn, "the expression result is null; it serializes as the text None");
        return "None";
    }
    if(pybind11::isinstance<pybind11::str>(result))
        return result.cast<std::string>();
    if(pybind11::isinstance<pybind11::bool_>(result))
        return *render_scalar(value{ result.cast<bool>() });
    if(pybind11::isinstance<pybind11::float_>(result))
    {
        const std::optional<value> number = value::make_real(result.cast<double>());
        if(!number)
            throw std::runtime_error("a non-finite float is not a serializable expression result");
        return *render_scalar(*number);
    }
    if(is_yaml_wrapper(priv, result))
        return yaml_container_marker + pybind11::str(result).cast<std::string>() + yaml_container_marker;
    if(pybind11::isinstance<pybind11::list>(result) || pybind11::isinstance<pybind11::dict>(result)
        || pybind11::isinstance<pybind11::tuple>(result))
        return plain_container_marker + pybind11::str(result).cast<std::string>() + plain_container_marker;
    if(pybind11::isinstance<pybind11::int_>(result))
        return pybind11::str(result).cast<std::string>();
    throw std::runtime_error("a " + pybind11::type::handle_of(result).attr("__name__").cast<std::string>()
        + " is not a serializable expression result");
}

}

#endif

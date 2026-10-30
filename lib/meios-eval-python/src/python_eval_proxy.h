#ifndef HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_PROXY_H
#define HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_PROXY_H

#include "python_eval_convert.h"
#include "python_eval_namespace.h"

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/scope_namespace.h"
#include "meios/xacro/eval_failure_kind.h"

#include "meios/xacro/detail/scope_read.h"
#include "meios/xacro/detail/scope_entry.h"

#include <pybind11/embed.h>

#include <string>
#include <optional>

namespace meios::detail
{

inline pybind11::object namespace_proxy(const pybind11::dict &priv, yaml_context &ctx, const scope_namespace &space);

inline pybind11::error_already_set raised(const pybind11::handle &type, const std::string &message)
{
    pybind11::set_error(type, message.c_str());
    return pybind11::error_already_set();
}

// A failed read has already logged its one record, so it is raised as an error no lookup takes for
// a missing key: Python would otherwise go on to a mathematics name or a builtin of the same
// spelling and render a wrong value in silence.
inline pybind11::object read_answer(const pybind11::dict &priv, yaml_context &ctx, const name_read &read)
{
    if(read.failed())
    {
        ctx.reported = read.failure;
        throw raised(priv["_ScopeReadFailed"], "a property this expression reads refused");
    }
    if(read.bound)
        return to_py_object(priv, *read.bound);
    return priv["_ABSENT"];
}

inline pybind11::object scope_name_reader(const pybind11::dict &priv, yaml_context &ctx)
{
    return pybind11::cpp_function(
            [priv, &ctx](const std::string &name)
            {
                const name_read read = read_name(ctx.scope, name);
                if(const std::optional<scope_namespace> space = read.space ? ctx.scope.namespace_of(name) : std::nullopt)
                    return namespace_proxy(priv, ctx, *space);
                return read_answer(priv, ctx, read);
            });
}

// An absent member raises what the reference's namespace raises for it.
inline pybind11::object namespace_member_reader(const pybind11::dict &priv, yaml_context &ctx, const scope_namespace &space)
{
    return pybind11::cpp_function(
            [priv, &ctx, space](const std::string &item)
            {
                const name_read read = read_member(ctx.scope, space, item);
                if(const std::optional<scope_namespace> inner = read.space ? ctx.scope.namespace_of(space, item) : std::nullopt)
                    return namespace_proxy(priv, ctx, *inner);
                pybind11::object found = read_answer(priv, ctx, read);
                if(found.is(priv["_ABSENT"]))
                    throw raised(PyExc_NameError, "name '" + item + "' is not defined");
                return found;
            });
}

inline pybind11::object namespace_proxy(const pybind11::dict &priv, yaml_context &ctx, const scope_namespace &space)
{
    pybind11::object proxy = priv["_ScopeNamespace"]();
    proxy.attr("_member")  = namespace_member_reader(priv, ctx, space);
    return proxy;
}

inline pybind11::dict fallback_names(const pybind11::dict &priv, yaml_context &ctx)
{
    pybind11::dict names    = priv["math_names"]().cast<pybind11::dict>();
    pybind11::object helper = yaml_helper(priv, ctx);
    names["load_yaml"]      = helper;
    names["xacro"]          = pybind11::module_::import("types").attr("SimpleNamespace")(pybind11::arg("load_yaml") = helper);
    return names;
}

// The interpreter injects the real builtins module into a globals mapping that lacks the key, so
// the chosen builtins are stored before anything is evaluated, as the mapping's one stored key.
inline pybind11::object scope_globals(const pybind11::dict &priv, yaml_context &ctx, const pybind11::object &builtins)
{
    pybind11::object globals  = priv["_ScopeGlobals"]();
    globals["__builtins__"]   = builtins;
    globals.attr("_read")     = scope_name_reader(priv, ctx);
    globals.attr("_fallback") = fallback_names(priv, ctx);
    return globals;
}

}

#endif

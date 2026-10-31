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
#include <cstddef>
#include <optional>

namespace meios::detail
{

inline pybind11::object namespace_proxy(const pybind11::dict &priv, yaml_context &ctx, const scope_namespace &space, const pybind11::tuple &fallback);

// The interpreter's frames stay on the stack beneath the evaluation a read starts, as a parser's
// do, so a read carries the nesting they are worth.
inline constexpr std::size_t python_read_nesting = 9;

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

inline pybind11::object scope_name_reader(const pybind11::dict &priv, yaml_context &ctx, const pybind11::tuple &fallback)
{
    return pybind11::cpp_function(
            [priv, &ctx, fallback](const std::string &name)
            {
                const name_read read = read_name(ctx.scope, name, python_read_nesting);
                if(const std::optional<scope_namespace> space = read.space ? ctx.scope.namespace_of(name) : std::nullopt)
                    return namespace_proxy(priv, ctx, *space, fallback);
                return read_answer(priv, ctx, read);
            });
}

inline pybind11::object namespace_member_reader(const pybind11::dict &priv, yaml_context &ctx, const scope_namespace &space, const pybind11::tuple &fallback)
{
    return pybind11::cpp_function(
            [priv, &ctx, space, fallback](const std::string &item)
            {
                const name_read read = read_member(ctx.scope, space, item, python_read_nesting);
                if(const std::optional<scope_namespace> inner = read.space ? ctx.scope.namespace_of(space, item) : std::nullopt)
                    return namespace_proxy(priv, ctx, *inner, fallback);
                return read_answer(priv, ctx, read);
            });
}

inline pybind11::object namespace_proxy(const pybind11::dict &priv, yaml_context &ctx, const scope_namespace &space, const pybind11::tuple &fallback)
{
    pybind11::object proxy           = priv["_ScopeNamespace"]();
    proxy.attr("__meios_member__")   = namespace_member_reader(priv, ctx, space, fallback);
    proxy.attr("__meios_fallback__") = fallback;
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
// A namespace answers a member no table binds from the mappings the globals answer a bare name from;
// the unrestricted backend holds its builtins as the module, whose mapping is its __dict__.
inline pybind11::object scope_globals(const pybind11::dict &priv, yaml_context &ctx, const pybind11::object &builtins)
{
    const pybind11::dict names   = fallback_names(priv, ctx);
    const pybind11::object table = pybind11::isinstance<pybind11::dict>(builtins) ? builtins : builtins.attr("__dict__");
    pybind11::object globals     = priv["_ScopeGlobals"]();
    globals["__builtins__"]      = builtins;
    globals.attr("_read")        = scope_name_reader(priv, ctx, pybind11::make_tuple(names, table));
    globals.attr("_fallback")    = names;
    return globals;
}

}

#endif

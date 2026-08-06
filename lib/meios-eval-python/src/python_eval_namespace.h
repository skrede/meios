#ifndef HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_NAMESPACE_H
#define HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_NAMESPACE_H

#include "guard_prelude.h"
#include "python_eval_convert.h"

#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pybind11/embed.h>

#include <cctype>
#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

constexpr std::string_view uncontained_rule = "uncontained-yaml-path";

struct yaml_context
{
    log_sink &log;
    const eval_scope &scope;
    std::optional<std::string> rule;
};

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

// The refusal is recorded in evaluation-local state before the throw, so the outer handler
// classifies it without inspecting the raised exception's text.
inline pybind11::object fetch_and_parse(const pybind11::dict &priv, const pybind11::object &parse,
                                        yaml_context &ctx, const std::string &spec)
{
    const std::optional<std::string> text = ctx.scope.load_text(spec);
    if(text)
        return wrap_yaml(priv, parse(pybind11::str(*text)));
    ctx.rule = std::string(uncontained_rule);
    throw pybind11::value_error("unreachable yaml resource \"" + spec + '"');
}

// A compiled callable carries no defining-globals attribute, so the privileged namespace
// holding the real builtins and the yaml module is not merely guarded against from the user
// namespace — it is absent from the reach of everything bound there.
inline pybind11::object yaml_helper(const pybind11::dict &priv, yaml_context &ctx)
{
    pybind11::object parse = priv["parse_yaml"];
    return pybind11::cpp_function(
        [priv, parse, &ctx](const std::string &spec) { return fetch_and_parse(priv, parse, ctx, spec); });
}

// These two are the whole axis of variation between the evaluators: everything else an
// expression can see is built identically for both.
inline pybind11::object curated_builtins(const pybind11::dict &priv) { return priv["allowed_builtins"](); }

inline pybind11::object real_builtins() { return pybind11::module_::import("builtins"); }

// pybind11 injects the real builtins module into any globals dict that lacks the key, on every
// eval and exec alike, so the chosen builtins have to be the dict's first entry or they are inert.
inline pybind11::dict user_globals(const pybind11::dict &priv, yaml_context &ctx, const pybind11::object &builtins)
{
    pybind11::dict globals;
    globals["__builtins__"] = builtins;
    globals.attr("update")(priv["math_names"]());
    pybind11::object helper = yaml_helper(priv, ctx);
    globals["load_yaml"]    = helper;
    globals["xacro"] = pybind11::module_::import("types").attr("SimpleNamespace")(pybind11::arg("load_yaml") = helper);
    return globals;
}

inline void seed_scope(const pybind11::dict &priv, pybind11::dict &globals, std::string_view expr,
                       const eval_scope &scope)
{
    for(const std::string &name : identifiers(expr))
    {
        std::optional<binding> bound = scope.lookup(name);
        if(bound)
            globals[pybind11::str(name)] = to_py_object(priv, *bound);
    }
}

}

#endif

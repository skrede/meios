#ifndef HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_NAMESPACE_H
#define HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_NAMESPACE_H

#include "guard_prelude.h"
#include "python_eval_convert.h"

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <pybind11/embed.h>

#include <string>
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
    std::optional<eval_failure_kind> reported;
};

inline pybind11::dict privileged_namespace()
{
    pybind11::dict priv;
    pybind11::exec(std::string(guard_prelude_source()), priv);
    return priv;
}

// Python hands the guard each name already normalized, so boundness is asked of the name Python
// reads, not of the written text.
inline pybind11::cpp_function bound_names(const eval_scope &scope)
{
    return pybind11::cpp_function([&scope](const std::string &name) { return scope.contains(name); });
}

inline std::string refusal_rule(const pybind11::dict &priv, std::string_view expr, const pybind11::object &bound)
{
    pybind11::object rule = priv["check_expression"](pybind11::str(std::string(expr)), bound);
    return rule.cast<std::string>();
}

inline eval_failure_kind report_unread(log_sink &log, const std::string &spec, text_resource_failure why)
{
    if(why == text_resource_failure::exhausted)
    {
        log.log(level::error, diagnostic_code::expansion_budget_exceeded, source_location{},
                "the auxiliary document \"" + spec + "\" is larger than its loader's read ceiling; this load cannot continue");
        return eval_failure_kind::exhausted;
    }
    log.log(level::error, diagnostic_code::unresolved_asset, source_location{}, "cannot reach the auxiliary document \"" + spec + '"');
    return eval_failure_kind::error;
}

// The cause is recorded in evaluation-local state before the throw, so the outer handler
// classifies it without inspecting the raised exception's text. A containment refusal is reported
// there under its rule; any other unread document is reported here, coded, as the failure's one
// record, and the handler returns its kind without adding another.
inline pybind11::object fetch_and_parse(const pybind11::dict &priv, const pybind11::object &parse, yaml_context &ctx, const std::string &spec)
{
    const text_resource_result text = ctx.scope.load_text(spec);
    if(text)
        return wrap_yaml(priv, parse(pybind11::str(*text)));
    if(text.error() == text_resource_failure::uncontained)
        ctx.rule = std::string(uncontained_rule);
    else
        ctx.reported = report_unread(ctx.log, spec, text.error());
    throw pybind11::value_error("cannot read the auxiliary document \"" + spec + '"');
}

// A compiled callable carries no defining-globals attribute, so the privileged namespace
// holding the real builtins and the yaml module is not merely guarded against from the user
// namespace — it is absent from the reach of everything bound there.
inline pybind11::object yaml_helper(const pybind11::dict &priv, yaml_context &ctx)
{
    pybind11::object parse = priv["parse_yaml"];
    return pybind11::cpp_function([priv, parse, &ctx](const std::string &spec) { return fetch_and_parse(priv, parse, ctx, spec); });
}

// These two are the whole axis of variation between the evaluators: everything else an
// expression can see is built identically for both.
inline pybind11::object curated_builtins(const pybind11::dict &priv)
{
    return priv["allowed_builtins"]();
}

inline pybind11::object real_builtins()
{
    return pybind11::module_::import("builtins");
}

}

#endif

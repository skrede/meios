#include "meios/eval/python_evaluator.h"

#include "interpreter.h"
#include "guard_prelude.h"
#include "python_eval_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/container_marker.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pybind11/embed.h>

#include <string>
#include <variant>
#include <optional>
#include <string_view>

namespace py = pybind11;

namespace meios
{

namespace
{

using detail::container_marker;

// Only a marker-wrapped string is re-parsed with ast.literal_eval and rebound to its real
// container; any other string is returned verbatim, so a literal-shaped author value like
// "[1, 2]" stays a string, matching xacro's _eval_literal.
py::object rehydrate(const std::string &text)
{
    if(text.size() < 2 || text.front() != container_marker || text.back() != container_marker)
        return py::str(text);
    const std::string inner = text.substr(1, text.size() - 2);
    try
    {
        py::object parsed = py::module_::import("ast").attr("literal_eval")(inner);
        if(py::isinstance<py::dict>(parsed) || py::isinstance<py::list>(parsed)
            || py::isinstance<py::tuple>(parsed))
            return parsed;
    }
    catch(py::error_already_set &)
    {
    }
    return py::str(inner);
}

py::object to_py_object(const binding &bound)
{
    if(std::holds_alternative<std::string>(bound))
        return rehydrate(std::get<std::string>(bound));
    const value &v = std::get<value>(bound);
    if(std::holds_alternative<bool>(v))
        return py::bool_(std::get<bool>(v));
    if(std::holds_alternative<long long>(v))
        return py::int_(std::get<long long>(v));
    return py::float_(std::get<double>(v));
}

constexpr std::string_view uncontained_rule = "uncontained-yaml-path";

struct yaml_context
{
    log_sink &log;
    const eval_scope &scope;
    std::optional<std::string> rule;
};

// The refusal is recorded in evaluation-local state before the throw, so the outer handler
// classifies it without inspecting the raised exception's text.
py::object fetch_and_parse(const py::object &parse, yaml_context &ctx, const std::string &spec)
{
    const std::optional<std::string> text = ctx.scope.load_text(spec);
    if(text)
        return parse(py::str(*text));
    ctx.rule = std::string(uncontained_rule);
    ctx.log.log(level::error,
                "yaml resource \"" + spec + "\" is not reachable inside a containment root");
    throw py::value_error("unreachable yaml resource \"" + spec + '"');
}

// A compiled callable carries no defining-globals attribute, so the privileged namespace
// holding the real builtins and the yaml module is not merely guarded against from the user
// namespace — it is absent from the reach of everything bound there.
py::object yaml_helper(const py::dict &priv, yaml_context &ctx)
{
    py::object parse = priv["parse_yaml"];
    return py::cpp_function([parse, &ctx](const std::string &spec) { return fetch_and_parse(parse, ctx, spec); });
}

// pybind11 injects the real builtins module into any globals dict that lacks the key, on every
// eval and exec alike, so the curated allowlist has to be the dict's first entry or it is inert.
py::dict restricted_globals(const py::dict &priv, yaml_context &ctx)
{
    py::dict globals;
    globals["__builtins__"] = priv["allowed_builtins"]();
    globals.attr("update")(priv["math_names"]());
    py::object helper    = yaml_helper(priv, ctx);
    globals["load_yaml"] = helper;
    globals["xacro"]     = py::module_::import("types").attr("SimpleNamespace")(py::arg("load_yaml") = helper);
    return globals;
}

void seed_scope(py::dict &globals, std::string_view expr, const eval_scope &scope)
{
    for(const std::string &name : detail::identifiers(expr))
    {
        std::optional<binding> bound = scope.lookup(name);
        if(bound)
            globals[py::str(name)] = to_py_object(*bound);
    }
}

// bool/float route back through the core evaluator's to_python_str so the two paths render
// identically (True/False, .0-append); str returns verbatim; int renders via Python's own
// str() to preserve arbitrary-precision results (${2**64}) that overflow long long, bool
// preceding int because Python bool subtypes int. A list/dict/tuple is delimited with the
// container_marker so rehydrate can round-trip it across a xacro:property boundary.
std::string format_result(const py::object &result)
{
    if(py::isinstance<py::str>(result))
        return result.cast<std::string>();
    if(py::isinstance<py::bool_>(result))
        return to_python_str(value{ result.cast<bool>() });
    if(py::isinstance<py::float_>(result))
        return to_python_str(value{ result.cast<double>() });
    if(py::isinstance<py::list>(result) || py::isinstance<py::dict>(result)
        || py::isinstance<py::tuple>(result))
        return container_marker + py::str(result).cast<std::string>() + container_marker;
    return py::str(result).cast<std::string>();
}

std::optional<std::string> report(eval_failure_kind &kind, eval_failure_kind which, log_sink &log,
                                  const std::string &message)
{
    kind = which;
    log.log(level::error, message);
    return std::nullopt;
}

std::optional<std::string> classify(eval_failure_kind &kind, const yaml_context &ctx,
                                    const std::string &quoted, const std::string &fallback)
{
    if(ctx.rule)
        return report(kind, eval_failure_kind::refused, ctx.log, "meios refused the expression " + quoted + ": " + *ctx.rule);
    return report(kind, eval_failure_kind::error, ctx.log, fallback);
}

py::object seed_and_eval(const py::dict &priv, std::string_view expr, yaml_context &ctx)
{
    py::dict globals = restricted_globals(priv, ctx);
    seed_scope(globals, expr, ctx.scope);
    return py::eval(std::string(expr), globals);
}

std::optional<std::string> eval_or_refuse(std::string_view expr, yaml_context &ctx,
                                          const std::string &quoted, eval_failure_kind &kind)
{
    const py::dict priv    = detail::privileged_namespace();
    const std::string rule = detail::refusal_rule(priv, expr, detail::bound_names(expr, ctx.scope));
    if(!rule.empty())
        return report(kind, eval_failure_kind::refused, ctx.log, "meios refused the expression " + quoted + ": " + rule);
    std::string text = format_result(seed_and_eval(priv, expr, ctx));
    kind = eval_failure_kind::none;
    return text;
}

}

std::optional<std::string> python_evaluator::eval_to_text(std::string_view expr,
                                                          const eval_scope &scope, log_sink &log)
{
    detail::ensure_interpreter();
    py::gil_scoped_acquire gil;
    const std::string quoted = "\"" + std::string(expr) + "\"";
    yaml_context ctx{ log, scope, std::nullopt };
    try
    {
        return eval_or_refuse(expr, ctx, quoted, m_kind);
    }
    catch(py::error_already_set &raised)
    {
        return classify(m_kind, ctx, quoted, "python evaluation of " + quoted + " raised: " + raised.what());
    }
    catch(const std::exception &raised)
    {
        return classify(m_kind, ctx, quoted, "python evaluation of " + quoted + " failed to convert its result: " + raised.what());
    }
}

}

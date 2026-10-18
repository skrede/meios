#ifndef HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_DETAIL_H
#define HPP_GUARD_MEIOS_EVAL_PYTHON_EVAL_DETAIL_H

#include "interpreter.h"
#include "python_eval_convert.h"
#include "python_eval_namespace.h"

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pybind11/embed.h>

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

inline std::string evaluate_in(const pybind11::dict &priv, std::string_view expr, yaml_context &ctx, const pybind11::object &builtins)
{
    pybind11::dict globals = user_globals(priv, ctx, builtins);
    seed_scope(priv, globals, expr, ctx.scope);
    return format_result(priv, ctx.log, pybind11::eval(std::string(expr), globals));
}

inline text_outcome report(eval_failure_kind which, log_sink &log, const std::string &message)
{
    log.log(level::error, message);
    return text_outcome{std::nullopt, which};
}

inline text_outcome classify(const yaml_context &ctx, const std::string &quoted, const std::string &fallback)
{
    if(ctx.reported)
        return text_outcome{std::nullopt, *ctx.reported};
    if(ctx.rule)
        return report(eval_failure_kind::refused, ctx.log, "meios refused the expression " + quoted + ": " + *ctx.rule);
    return report(eval_failure_kind::error, ctx.log, fallback);
}

// A NameError carries the unbound name as its name attribute only from Python 3.10 on.
inline std::string argument_hint(const pybind11::error_already_set &raised, const eval_scope &scope)
{
    if(!raised.matches(PyExc_NameError) || !pybind11::hasattr(raised.value(), "name"))
        return std::string{};
    const pybind11::object name = raised.value().attr("name");
    if(!pybind11::isinstance<pybind11::str>(name))
        return std::string{};
    const std::string text = name.cast<std::string>();
    if(!scope.has_argument(text))
        return std::string{};
    return " — '" + text + "' is an argument; read it with $(arg " + text + ")";
}

// Interpreter readiness, the interpreter lock and the failure classification are one
// implementation for both evaluators; the body is the only thing that differs between them.
template<typename Body>
text_outcome under_interpreter(std::string_view expr, const eval_scope &scope, log_sink &log, Body body)
{
    ensure_interpreter();
    pybind11::gil_scoped_acquire gil;
    const std::string quoted = "\"" + std::string(expr) + "\"";
    yaml_context ctx{log, scope, std::nullopt, std::nullopt};
    try
    {
        text_outcome out = body(expr, ctx, quoted);
        // An expression that caught the helper's raise still ends on the failure the helper recorded.
        return ctx.reported || ctx.rule ? classify(ctx, quoted, std::string{}) : out;
    }
    catch(pybind11::error_already_set &raised)
    {
        return classify(ctx, quoted, "python evaluation of " + quoted + " raised: " + raised.what() + argument_hint(raised, scope));
    }
    catch(const std::exception &raised)
    {
        return classify(ctx, quoted, "python evaluation of " + quoted + " failed to convert its result: " + raised.what());
    }
}

}

#endif

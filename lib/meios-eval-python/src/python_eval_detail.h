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

inline std::string evaluate_in(const pybind11::dict &priv, std::string_view expr, yaml_context &ctx,
                               const pybind11::object &builtins)
{
    pybind11::dict globals = user_globals(priv, ctx, builtins);
    seed_scope(globals, expr, ctx.scope);
    return format_result(pybind11::eval(std::string(expr), globals));
}

inline std::optional<std::string> report(eval_failure_kind &kind, eval_failure_kind which, log_sink &log,
                                         const std::string &message)
{
    kind = which;
    log.log(level::error, message);
    return std::nullopt;
}

inline std::optional<std::string> classify(eval_failure_kind &kind, const yaml_context &ctx,
                                           const std::string &quoted, const std::string &fallback)
{
    if(ctx.rule)
        return report(kind, eval_failure_kind::refused, ctx.log, "meios refused the expression " + quoted + ": " + *ctx.rule);
    return report(kind, eval_failure_kind::error, ctx.log, fallback);
}

// Interpreter readiness, the interpreter lock and the failure classification are one
// implementation for both evaluators; the body is the only thing that differs between them.
template <typename Body>
std::optional<std::string> under_interpreter(std::string_view expr, const eval_scope &scope, log_sink &log,
                                             eval_failure_kind &kind, Body body)
{
    ensure_interpreter();
    pybind11::gil_scoped_acquire gil;
    const std::string quoted = "\"" + std::string(expr) + "\"";
    yaml_context ctx{ log, scope, std::nullopt };
    try
    {
        return body(expr, ctx, quoted, kind);
    }
    catch(pybind11::error_already_set &raised)
    {
        return classify(kind, ctx, quoted, "python evaluation of " + quoted + " raised: " + raised.what());
    }
    catch(const std::exception &raised)
    {
        return classify(kind, ctx, quoted, "python evaluation of " + quoted + " failed to convert its result: " + raised.what());
    }
}

}

#endif

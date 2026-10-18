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
#include <cstddef>
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

// The unbound name is read from the error's own text, which carries it on every interpreter
// version; the attribute naming it is set only from Python 3.10 on, and only by the interpreter.
inline std::string argument_hint(bool unbound, std::string_view raised, const eval_scope &scope)
{
    constexpr std::string_view opener = "name '";
    const std::size_t at              = unbound ? raised.find(opener) : std::string_view::npos;
    const std::size_t end             = at == std::string_view::npos ? at : raised.find("' is not defined", at);
    if(end == std::string_view::npos)
        return std::string{};
    const std::string name(raised.substr(at + opener.size(), end - at - opener.size()));
    if(!scope.has_argument(name))
        return std::string{};
    return " — '" + name + "' is an argument; read it with $(arg " + name + ")";
}

// Interpreter readiness, the interpreter lock and the failure classification are one
// implementation for both evaluators; the body is the only thing that differs between them. A
// raised error is only read inside its handler, so nothing the message is built from can throw there.
template<typename Body>
text_outcome under_interpreter(std::string_view expr, const eval_scope &scope, log_sink &log, Body body)
{
    ensure_interpreter();
    pybind11::gil_scoped_acquire gil;
    const std::string quoted = "\"" + std::string(expr) + "\"";
    yaml_context ctx{log, scope, std::nullopt, std::nullopt};
    std::string message;
    bool unbound = false;
    try
    {
        text_outcome out = body(expr, ctx, quoted);
        // An expression that caught the helper's raise still ends on the failure the helper recorded.
        return ctx.reported || ctx.rule ? classify(ctx, quoted, std::string{}) : out;
    }
    catch(pybind11::error_already_set &raised)
    {
        unbound = raised.matches(PyExc_NameError);
        message = raised.what();
    }
    catch(const std::exception &raised)
    {
        return classify(ctx, quoted, "python evaluation of " + quoted + " failed to convert its result: " + raised.what());
    }
    return classify(ctx, quoted, "python evaluation of " + quoted + " raised: " + message + argument_hint(unbound, message, scope));
}

}

#endif

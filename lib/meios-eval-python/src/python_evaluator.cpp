#include "meios/eval/python_evaluator.h"

#include "python_eval_detail.h"

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/log_sink.h"

#include <pybind11/embed.h>

#include <string>
#include <optional>
#include <string_view>

namespace py = pybind11;

namespace meios
{

namespace
{

std::optional<std::string> eval_or_refuse(std::string_view expr, detail::yaml_context &ctx,
                                          const std::string &quoted, eval_failure_kind &kind)
{
    const py::dict priv    = detail::privileged_namespace();
    const std::string rule = detail::refusal_rule(priv, expr, detail::bound_names(expr, ctx.scope));
    if(!rule.empty())
        return detail::report(kind, eval_failure_kind::refused, ctx.log,
                              "meios refused the expression " + quoted + ": " + rule);
    std::string text = detail::evaluate_in(priv, expr, ctx, detail::curated_builtins(priv));
    kind             = eval_failure_kind::none;
    return text;
}

}

std::optional<std::string> python_evaluator::eval_to_text(std::string_view expr,
                                                          const eval_scope &scope, log_sink &log)
{
    return detail::under_interpreter(expr, scope, log, m_kind, eval_or_refuse);
}

}

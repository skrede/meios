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

text_outcome eval_or_refuse(std::string_view expr, detail::yaml_context &ctx,
                            const std::string &quoted)
{
    const py::dict priv    = detail::privileged_namespace();
    const std::string rule = detail::refusal_rule(priv, expr, detail::bound_names(expr, ctx.scope));
    if(!rule.empty())
        return detail::report(eval_failure_kind::refused, ctx.log,
                              "meios refused the expression " + quoted + ": " + rule);
    return text_outcome{ detail::evaluate_in(priv, expr, ctx, detail::curated_builtins(priv)),
                         eval_failure_kind::none };
}

}

text_outcome python_evaluator::eval_to_text(std::string_view expr, const eval_scope &scope,
                                            log_sink &log)
{
    return detail::under_interpreter(expr, scope, log, eval_or_refuse);
}

}

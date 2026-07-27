#include "meios/eval/unrestricted_python_evaluator.h"

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

std::optional<std::string> eval_unguarded(std::string_view expr, detail::yaml_context &ctx,
                                          const std::string &, eval_failure_kind &kind)
{
    const py::dict priv = detail::privileged_namespace();
    std::string text    = detail::evaluate_in(priv, expr, ctx, detail::real_builtins());
    kind                = eval_failure_kind::none;
    return text;
}

}

std::optional<std::string> unrestricted_python_evaluator::eval_to_text(std::string_view expr,
                                                                      const eval_scope &scope,
                                                                      log_sink &log)
{
    return detail::under_interpreter(expr, scope, log, m_kind, eval_unguarded);
}

}

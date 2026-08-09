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

text_outcome eval_unguarded(std::string_view expr, detail::yaml_context &ctx, const std::string &)
{
    const py::dict priv = detail::privileged_namespace();
    return text_outcome{ detail::evaluate_in(priv, expr, ctx, detail::real_builtins()),
                         eval_failure_kind::none };
}

}

text_outcome unrestricted_python_evaluator::eval_to_text(std::string_view expr,
                                                         const eval_scope &scope, log_sink &log)
{
    return detail::under_interpreter(expr, scope, log, eval_unguarded);
}

}

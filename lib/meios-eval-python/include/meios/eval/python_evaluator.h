#ifndef HPP_GUARD_MEIOS_EVAL_PYTHON_EVALUATOR_H
#define HPP_GUARD_MEIOS_EVAL_PYTHON_EVALUATOR_H

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios
{

// Restricted ${python}/$(eval) backend over an embedded, found (never fetched) CPython;
// docs/evaluation.md states the subset it evaluates and the rules that refuse the rest.
// It satisfies the string-level text_evaluator seam, so it injects through load_options
// with no core dependency. The interpreter is a process-lifetime lazy singleton (the GIL
// and interpreter are global process state); a runtime Python throw is reported as an
// error and a refused expression as a refusal, and eval_policy softens neither.
class python_evaluator
{
public:
    python_evaluator() : m_kind(eval_failure_kind::none) {}

    std::optional<std::string> eval_to_text(std::string_view expr, const eval_scope &scope, log_sink &log);

    eval_failure_kind last_failure_kind() const { return m_kind; }

private:
    eval_failure_kind m_kind;
};

}

#endif

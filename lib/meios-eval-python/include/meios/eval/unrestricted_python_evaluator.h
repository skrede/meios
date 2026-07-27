#ifndef HPP_GUARD_MEIOS_EVAL_UNRESTRICTED_PYTHON_EVALUATOR_H
#define HPP_GUARD_MEIOS_EVAL_UNRESTRICTED_PYTHON_EVALUATOR_H

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios
{

// The same backend as python_evaluator without the expression guard: an expression in a
// description evaluates with the process's own authority, so this is for descriptions a
// consumer would be willing to run as a script. Resource resolution and containment are
// the restricted evaluator's own — the same URI forms resolve and the same escapes are
// refused — so it is a wider expression surface, not a way past the filesystem rules. The
// interpreter is the same process-lifetime lazy singleton holding the interpreter lock.
// Selecting it means naming this header and constructing this class, which no
// command-line surface does.
class unrestricted_python_evaluator
{
public:
    unrestricted_python_evaluator() : m_kind(eval_failure_kind::none) {}

    std::optional<std::string> eval_to_text(std::string_view expr, const eval_scope &scope, log_sink &log);

    eval_failure_kind last_failure_kind() const { return m_kind; }

private:
    eval_failure_kind m_kind;
};

}

#endif

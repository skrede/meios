#ifndef HPP_GUARD_MEIOS_XACRO_CORE_EVALUATOR_H
#define HPP_GUARD_MEIOS_XACRO_CORE_EVALUATOR_H

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios
{

namespace detail
{
struct eval_session;
}

// Separates a construct the core cannot evaluate but a fuller backend could (unsupported, the
// only kind a lenient eval_policy may soften) from a genuine evaluation fault (error) such as
// an undefined name, from one the evaluator declines to run (refused), and from a crossed
// resource ceiling (exhausted). Every kind but unsupported is terminal under every policy.
enum class eval_failure_kind
{
    none,
    unsupported,
    error,
    refused,
    exhausted,
};

// The failure kind travels back with the result of the call that produced it, so an
// evaluator reachable from two concurrent loads keeps no failure state to read.
struct text_outcome
{
    std::optional<std::string> text;
    eval_failure_kind failure;
};

class core_evaluator
{
public:
    core_evaluator() : m_failed(false), m_kind(eval_failure_kind::none) {}

    value eval(std::string_view expression, const eval_scope &scope, log_sink &log,
               const source_location &at = {});

    // The overload a load drives: the session carries the ceilings and the counters that
    // accumulate across every expression in that load, and is never shared with another.
    value eval(std::string_view expression, const eval_scope &scope, log_sink &log,
               const source_location &at, detail::eval_session &session);

    bool failed() const { return m_failed; }

    eval_failure_kind failure_kind() const { return m_kind; }

private:
    bool              m_failed;
    eval_failure_kind m_kind;
};

}

#endif

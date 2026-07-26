#ifndef HPP_GUARD_MEIOS_XACRO_CORE_EVALUATOR_H
#define HPP_GUARD_MEIOS_XACRO_CORE_EVALUATOR_H

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <string_view>

namespace meios
{

// Separates a construct the core cannot evaluate but a fuller backend could (unsupported, the
// only kind a lenient eval_policy may soften) from a genuine evaluation fault (error) such as
// an undefined name, and from one the evaluator declines to run (refused, always hard).
enum class eval_failure_kind
{
    none,
    unsupported,
    error,
    refused,
};

class core_evaluator
{
public:
    core_evaluator() : m_failed(false), m_kind(eval_failure_kind::none) {}

    value eval(std::string_view expression, const eval_scope &scope, log_sink &log,
               const source_location &at = {});

    bool failed() const { return m_failed; }

    eval_failure_kind failure_kind() const { return m_kind; }

private:
    bool              m_failed;
    eval_failure_kind m_kind;
};

}

#endif

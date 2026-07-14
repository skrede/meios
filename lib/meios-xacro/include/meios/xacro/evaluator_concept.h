#ifndef HPP_GUARD_MEIOS_XACRO_EVALUATOR_CONCEPT_H
#define HPP_GUARD_MEIOS_XACRO_EVALUATOR_CONCEPT_H

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <concepts>
#include <string_view>

namespace meios
{

template <typename E>
concept expression_evaluator = requires(
    E &evaluator,
    std::string_view expr,
    const eval_scope &scope,
    log_sink &log)
{
    { evaluator.eval(expr, scope, log) } -> std::convertible_to<value>;
};

// Detected (never required): an evaluator that can name where in the source a
// failed expression sat, rather than only signaling that it failed.
template <typename E>
concept locates_errors =
    expression_evaluator<E> &&
    requires(const E evaluator)
    {
        { evaluator.last_error_location() } -> std::convertible_to<source_location>;
    };

}

#endif

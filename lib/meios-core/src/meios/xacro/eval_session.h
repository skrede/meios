#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_SESSION_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_SESSION_H

#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/evaluator_limits.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"

#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

// Every counter and every failure fact one load produces lives here, so the evaluator
// configuration a caller may share between two loads owns none of it. Each charge runs
// before the work it bounds and answers false when the ceiling is crossed, which latches
// an exhausted failure no evaluation policy may soften.
struct eval_session
{
    explicit eval_session(const evaluator_limits &ceilings)
        : limits(ceilings), counters(), span_depth(0), failure(eval_failure_kind::none),
          terminal()
    {
    }

    const evaluator_limits &limits;
    evaluator_counters counters;
    // How many substitution spans the scan currently stands inside. It is carried here rather
    // than on the scanner's own state because an argument default resolves through a
    // substitution context of its own: a depth held on that context restarts at every argument
    // boundary, so an argument nested inside its own default would recurse until the stack is
    // gone.
    std::size_t span_depth;
    eval_failure_kind failure;
    std::optional<expansion_error> terminal;

    bool charge_bytes(std::size_t amount, log_sink &log, const source_location &at);
    bool charge_tokens(std::size_t amount, log_sink &log, const source_location &at);
    bool charge_steps(std::size_t amount, log_sink &log, const source_location &at);
    bool charge_yaml_nodes(std::size_t amount, log_sink &log, const source_location &at);
    bool admits_yaml_depth(std::size_t depth, log_sink &log, const source_location &at);
    bool admits_expression_depth(std::size_t depth, log_sink &log, const source_location &at);
    bool admits_expression_tokens(std::size_t count, log_sink &log, const source_location &at);
    bool admits_string_length(std::size_t length, log_sink &log, const source_location &at);
    bool admits_numeric_magnitude(double magnitude, log_sink &log, const source_location &at);
    bool enter_span(std::size_t cost, log_sink &log, const source_location &at);
    void leave_span(std::size_t cost);

private:
    bool charge(std::size_t &counter, std::size_t ceiling, std::size_t amount,
                std::string_view axis, log_sink &log, const source_location &at);
    bool admits(std::size_t &counter, std::size_t ceiling, std::size_t depth,
                std::string_view axis, log_sink &log, const source_location &at);
};

void latch_exhausted(eval_session &session, std::string_view axis, log_sink &log,
                     const source_location &at);

}

#endif

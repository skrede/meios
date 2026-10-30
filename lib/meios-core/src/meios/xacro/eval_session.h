#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_SESSION_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_SESSION_H

#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/evaluator_limits.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <functional>
#include <string_view>

namespace meios::detail
{

struct read_anchor
{
    source_location at;
    std::reference_wrapper<log_sink> sink;
};

// Every counter and every failure fact one load produces lives here, so the evaluator
// configuration a caller may share between two loads owns none of it. Each charge runs
// before the work it bounds and answers false when the ceiling is crossed, which latches
// an exhausted failure no evaluation policy may soften.
struct eval_session
{
    explicit eval_session(const evaluator_limits &ceilings)
            : limits(ceilings)
            , counters()
            , span_depth(0)
            , failure(eval_failure_kind::none)
            , terminal()
            , anchors()
    {
    }

    // The ceilings are stored by reference and read for the whole load, so a temporary bound
    // here is already dead at the first charge. Deleted rather than documented: the spelling is
    // silent at compile time and undefined at run time.
    eval_session(const evaluator_limits &&) = delete;

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
    // The substitutions reading right now, innermost last. A deferred property resolved beneath
    // one names it as the read site and reports a refusal into its sink, so the reading layer
    // receives one record whichever depth refused.
    std::vector<read_anchor> anchors;

    bool charge_bytes(std::size_t amount, log_sink &log, const source_location &at);
    bool charge_tokens(std::size_t amount, log_sink &log, const source_location &at);
    bool charge_steps(std::size_t amount, log_sink &log, const source_location &at);
    bool charge_yaml_nodes(std::size_t amount, log_sink &log, const source_location &at);
    bool charge_elements(std::size_t amount, log_sink &log, const source_location &at);
    bool admits_yaml_depth(std::size_t depth, log_sink &log, const source_location &at);
    bool admits_expression_depth(std::size_t depth, log_sink &log, const source_location &at);
    bool admits_expression_tokens(std::size_t count, log_sink &log, const source_location &at);
    bool admits_string_length(std::size_t length, log_sink &log, const source_location &at);
    bool admits_numeric_magnitude(double magnitude, log_sink &log, const source_location &at);
    bool enter_span(std::size_t cost, log_sink &log, const source_location &at);
    void leave_span(std::size_t cost);
    std::optional<read_anchor> reading() const;

private:
    bool charge(std::size_t &counter, std::size_t ceiling, std::size_t amount, std::string_view axis, log_sink &log, const source_location &at);
    bool admits(std::size_t &counter, std::size_t ceiling, std::size_t depth, std::string_view axis, log_sink &log, const source_location &at);
};

// Holds the substitution's anchor for as long as it reads, and drops it on every return path.
class anchor_guard
{
public:
    anchor_guard(eval_session &session, const source_location &at, log_sink &sink);
    anchor_guard(const anchor_guard &) = delete;
    anchor_guard &operator=(const anchor_guard &) = delete;
    ~anchor_guard();

private:
    eval_session &m_session;
};

void latch_exhausted(eval_session &session, std::string_view axis, log_sink &log, const source_location &at);

// Latches the same terminal failure under a message of the caller's own, for a crossed limit that is
// not one of the evaluator's axes and so must be named by whoever holds it.
void report_exhausted(eval_session &session, const std::string &message, log_sink &log, const source_location &at);

}

#endif

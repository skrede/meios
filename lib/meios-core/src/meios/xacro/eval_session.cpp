#include "eval_session.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <cstddef>
#include <string_view>

namespace meios::detail
{

void latch_exhausted(eval_session &session, std::string_view axis, log_sink &log,
                     const source_location &at)
{
    const std::string message = "the evaluator's " + std::string(axis)
                              + " ceiling was reached; this load cannot continue";
    if(session.failure != eval_failure_kind::exhausted)
    {
        log.log(level::error, diagnostic_code::expansion_budget_exceeded, at, message);
        session.terminal =
            expansion_error{ at, message, diagnostic_code::expansion_budget_exceeded, std::nullopt };
    }
    session.failure = eval_failure_kind::exhausted;
}

bool eval_session::charge(std::size_t &counter, std::size_t ceiling, std::size_t amount,
                          std::string_view axis, log_sink &log, const source_location &at)
{
    if(failure == eval_failure_kind::exhausted || amount > ceiling - counter)
    {
        latch_exhausted(*this, axis, log, at);
        return false;
    }
    counter += amount;
    return true;
}

bool eval_session::charge_bytes(std::size_t amount, log_sink &log, const source_location &at)
{
    return charge(counters.bytes, limits.bytes, amount, "byte", log, at);
}

bool eval_session::charge_tokens(std::size_t amount, log_sink &log, const source_location &at)
{
    return charge(counters.tokens, limits.tokens, amount, "token", log, at);
}

bool eval_session::charge_steps(std::size_t amount, log_sink &log, const source_location &at)
{
    return charge(counters.steps, limits.steps, amount, "evaluation step", log, at);
}

bool eval_session::charge_yaml_nodes(std::size_t amount, log_sink &log, const source_location &at)
{
    return charge(counters.yaml_nodes, limits.yaml_nodes, amount, "auxiliary node", log, at);
}

bool eval_session::charge_elements(std::size_t amount, log_sink &log, const source_location &at)
{
    return charge(counters.elements, limits.elements, amount, "collection element", log, at);
}

// Depth is a high-water mark rather than a running total: reaching a nesting level is what
// the ceiling bounds, not how many times it is reached.
bool eval_session::admits(std::size_t &counter, std::size_t ceiling, std::size_t depth,
                          std::string_view axis, log_sink &log, const source_location &at)
{
    if(failure == eval_failure_kind::exhausted || depth > ceiling)
    {
        latch_exhausted(*this, axis, log, at);
        return false;
    }
    if(depth > counter)
        counter = depth;
    return true;
}

bool eval_session::admits_yaml_depth(std::size_t depth, log_sink &log, const source_location &at)
{
    return admits(counters.yaml_depth, limits.yaml_depth, depth, "auxiliary depth", log, at);
}

bool eval_session::admits_expression_depth(std::size_t depth, log_sink &log,
                                           const source_location &at)
{
    return admits(counters.expression_depth, limits.expression_depth, depth, "expression depth",
                  log, at);
}

bool eval_session::admits_expression_tokens(std::size_t count, log_sink &log,
                                            const source_location &at)
{
    return admits(counters.expression_tokens, limits.expression_tokens, count, "expression token",
                  log, at);
}

bool eval_session::admits_string_length(std::size_t length, log_sink &log,
                                        const source_location &at)
{
    return admits(counters.string_length, limits.string_length, length, "string length", log, at);
}

// The operand is compared as a real against the ceiling widened to one: narrowing it to a whole
// number first would truncate a magnitude just above the bound into admission. Only a magnitude
// already known to be within the bound reaches the counter, and a not-a-number widens neither.
bool eval_session::admits_numeric_magnitude(double magnitude, log_sink &log,
                                            const source_location &at)
{
    if(failure == eval_failure_kind::exhausted
       || magnitude > static_cast<double>(limits.numeric_magnitude))
    {
        latch_exhausted(*this, "numeric magnitude", log, at);
        return false;
    }
    if(magnitude > static_cast<double>(counters.numeric_magnitude))
        counters.numeric_magnitude = static_cast<std::size_t>(magnitude);
    return true;
}

bool eval_session::enter_span(std::size_t cost, log_sink &log, const source_location &at)
{
    if(!admits_expression_depth(span_depth + cost, log, at))
        return false;
    span_depth += cost;
    return true;
}

void eval_session::leave_span(std::size_t cost)
{
    span_depth -= cost;
}

}

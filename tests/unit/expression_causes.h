#ifndef HPP_GUARD_MEIOS_UNIT_EXPRESSION_CAUSES_H
#define HPP_GUARD_MEIOS_UNIT_EXPRESSION_CAUSES_H

#include <meios/diagnostic/diagnostic_code.h>

#include <meios/xacro/eval_failure_kind.h>

#include <string>
#include <vector>
#include <optional>
#include <string_view>

namespace oracle
{

struct expression_cause
{
    std::string_view stem;
    meios::eval_failure_kind kind;
    meios::diagnostic_code code;
    std::string_view naming;
};

// Upstream's recorded line is classified as tests/refusal_causes.h classifies a document's, and this side is held to its own failure for that cause.
inline constexpr expression_cause expression_causes[]{
        {"list index out of range", meios::eval_failure_kind::error, meios::diagnostic_code::undefined_property, "is outside the sequence"},
        {"list indices must be integers", meios::eval_failure_kind::error, meios::diagnostic_code::expression_error, "a sequence index must be an integer"},
        {"'", meios::eval_failure_kind::error, meios::diagnostic_code::undefined_property, "is not present"},
        {"keyword argument repeated", meios::eval_failure_kind::error, meios::diagnostic_code::expression_error, "a keyword argument is repeated"},
        {"expected argument value expression", meios::eval_failure_kind::error, meios::diagnostic_code::expression_error, "unexpected ')'"},
        {"can only concatenate str", meios::eval_failure_kind::unsupported, meios::diagnostic_code::unsupported_expression, "concatenates only with another string"},
        {"unsupported operand type(s) for +:", meios::eval_failure_kind::unsupported, meios::diagnostic_code::unsupported_expression, "concatenates only with another string"},
        {"unsupported operand type(s) for", meios::eval_failure_kind::unsupported, meios::diagnostic_code::unsupported_expression, "has no arithmetic meaning here"},
        {"not all arguments converted during string formatting", meios::eval_failure_kind::unsupported, meios::diagnostic_code::unsupported_expression,
         "has no arithmetic meaning here"},
        {"'in <string>' requires string as left operand", meios::eval_failure_kind::error, meios::diagnostic_code::expression_error, "only a string is contained in a string"},
        {"empty separator", meios::eval_failure_kind::error, meios::diagnostic_code::expression_error, "an empty separator does not split"},
        {"must be str or None", meios::eval_failure_kind::error, meios::diagnostic_code::expression_error, "the separator must be text"},
        {"division by zero", meios::eval_failure_kind::error, meios::diagnostic_code::expression_error, "the divisor of '/' is zero"},
        {"The yaml dictionary has no key", meios::eval_failure_kind::error, meios::diagnostic_code::undefined_property, "is not present"},
};

inline std::optional<expression_cause> cause_of(std::string_view recorded)
{
    std::optional<expression_cause> best;
    for(const expression_cause &row : expression_causes)
    {
        if(recorded.starts_with(row.stem) && (!best || row.stem.size() > best->stem.size()))
            best = row;
    }
    return best;
}

inline bool refuses_as_recorded(std::string_view recorded, meios::eval_failure_kind kind, const std::vector<meios::diagnostic_code> &codes, const std::vector<std::string> &messages)
{
    const std::optional<expression_cause> cause = cause_of(recorded);
    if(!cause || kind != cause->kind || codes.size() != 1 || codes.front() != cause->code)
        return false;
    for(const std::string &message : messages)
        if(message.find(cause->naming) != std::string::npos)
            return true;
    return false;
}

}

#endif

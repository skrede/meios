#ifndef HPP_GUARD_MEIOS_UNIT_CEILING_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_CEILING_PROBE_H

#include "meios/xacro/eval_session.h"
#include "meios/xacro/substitution_detail.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <string>
#include <vector>
#include <cstddef>
#include <functional>
#include <string_view>

namespace ceiling
{

struct recorder
{
    std::vector<std::string> messages;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        (*this)(lvl, message);
    }
};

struct outcome
{
    bool failed;
    std::string rendered;
    meios::eval_failure_kind kind;
    std::vector<std::string> messages;

    bool refused_on(std::string_view axis) const
    {
        return failed && kind == meios::eval_failure_kind::exhausted && messages.size() == 1
            && messages.front().find(axis) != std::string::npos;
    }
};

// Every case drives a session the case itself configured, so a ceiling can be set one unit under
// what the expression needs and the axis that answered can be read off the message.
inline outcome evaluate(std::string_view expression, meios::detail::eval_session &session)
{
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const meios::eval_scope scope;
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink, {}, session);
    return outcome{ evaluator.failed(), meios::render_scalar(result).value_or(std::string()),
                    evaluator.failure_kind(), heard.messages };
}

struct resolved
{
    bool survived;
    std::string text;
    std::vector<std::string> messages;
};

inline resolved substitute(std::string_view raw, meios::eval_policy policy,
                           meios::detail::eval_session &session)
{
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    meios::eval_scope scope;
    meios::source_stack sources;
    const meios::expected<meios::substitution, meios::expansion_error> out =
        meios::detail::substitute_refined(raw, scope, sources, "inline.xacro", policy, {}, session,
                                          sink, {});
    return resolved{ out.has_value(), out ? out->text : std::string(), heard.messages };
}

inline std::string nested_arguments(std::size_t levels)
{
    std::string text = "x";
    for(std::size_t at = 0; at < levels; ++at)
        text = "$(arg unset " + text + ")";
    return text;
}

inline std::string nested_commands(std::size_t levels)
{
    std::string text = "x";
    for(std::size_t at = 0; at < levels; ++at)
        text = "$(dirname " + text + ")";
    return text;
}

}

#endif

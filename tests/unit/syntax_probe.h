#ifndef HPP_GUARD_MEIOS_UNIT_SYNTAX_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_SYNTAX_PROBE_H

#include "branch_probe.h"

#include <meios/io/source_stack.h>

#include <string>
#include <functional>
#include <string_view>

namespace syntax_probe
{

// A refusal abandons whatever the prefix had produced, so the null spelling standing where a
// number or a boolean stood is what distinguishes a refusal from a value that reached the
// document.
inline bool refused(const probe::outcome &ran)
{
    return ran.failed && ran.rendered == "None";
}

inline meios::expected<meios::expansion, meios::expansion_error> load_under(std::string_view body, meios::eval_policy policy)
{
    const std::string document = std::string("<robot xmlns:xacro=\"http://ros.org/wiki/xacro\">") + "<l>" + std::string(body) + "</l></robot>";
    probe::recorder heard;
    meios::log_sink_f sink{std::ref(heard)};
    meios::eval_scope scope;
    meios::source_stack sources;
    return meios::expand(document, scope, sources, "robot.xacro", meios::expansion_limits{}, policy, {}, sink);
}

inline bool loads(std::string_view body, meios::eval_policy policy)
{
    return load_under(body, policy).has_value();
}

struct quoted_at
{
    std::string_view text;
    std::string_view lexeme;
};

}

#endif

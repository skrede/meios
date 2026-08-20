#include "value_builder.h"
#include "scalar_resolver.h"

#include "meios/xacro/value.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <yaml-cpp/mark.h>
#include <yaml-cpp/anchor.h>
#include <yaml-cpp/emitterstyle.h>

#include <string>
#include <cstddef>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail
{
namespace
{

// The mark is the position inside the auxiliary document, which no source_location can carry:
// the seam receives bytes and never learns the path they came from.
std::string at_mark(const YAML::Mark &mark)
{
    return " at line " + std::to_string(mark.line + 1) + ", column "
         + std::to_string(mark.column + 1);
}

}

value_builder::value_builder(const evaluator_limits &ceilings, evaluator_counters &tally,
                             log_sink &sink, const source_location &origin)
    : m_log(sink), m_at(origin), m_root(), m_frames(), m_counters(tally), m_limits(ceilings)
{
}

void value_builder::OnDocumentStart(const YAML::Mark &) {}

void value_builder::OnDocumentEnd() {}

void value_builder::OnNull(const YAML::Mark &mark, YAML::anchor_t)
{
    charge_node();
    if(expecting_key())
        throw stopped("a non-string mapping key", mark, yaml_failure::unsupported);
    deliver(value{}, mark);
}

void value_builder::OnAlias(const YAML::Mark &mark, YAML::anchor_t)
{
    throw stopped("an alias", mark, yaml_failure::unsupported);
}

void value_builder::OnScalar(const YAML::Mark &mark, const std::string &tag, YAML::anchor_t,
                             const std::string &text)
{
    charge_node();
    if(expecting_key())
        return take_key(text, tag, mark);
    deliver(resolved(text, tag, mark), mark);
}

void value_builder::OnSequenceStart(const YAML::Mark &mark, const std::string &tag, YAML::anchor_t,
                                    YAML::EmitterStyle::value)
{
    charge_node();
    admit_depth(m_frames.size() + 1);
    if(expecting_key())
        throw stopped("a sequence as a mapping key", mark, yaml_failure::unsupported);
    if(tag != "?" && tag != "!")
        throw stopped("an unsupported tag '" + tag + '\'', mark, yaml_failure::unsupported);
    m_frames.push_back(frame{ .listing = true, .at = mark });
}

void value_builder::OnSequenceEnd()
{
    frame done = std::move(m_frames.back());
    m_frames.pop_back();
    deliver(value::make_sequence(std::move(done.items)), done.at);
}

void value_builder::OnMapStart(const YAML::Mark &mark, const std::string &tag, YAML::anchor_t,
                               YAML::EmitterStyle::value)
{
    charge_node();
    admit_depth(m_frames.size() + 1);
    if(expecting_key())
        throw stopped("a non-string mapping key", mark, yaml_failure::unsupported);
    if(tag != "?" && tag != "!")
        throw stopped("an unsupported tag '" + tag + '\'', mark, yaml_failure::unsupported);
    m_frames.push_back(frame{ .listing = false, .at = mark });
}

void value_builder::OnMapEnd()
{
    frame done = std::move(m_frames.back());
    m_frames.pop_back();
    deliver(value::make_mapping(std::move(done.entries)), done.at);
}

value value_builder::result() const
{
    return m_root.value_or(value{});
}

bool value_builder::expecting_key() const
{
    return !m_frames.empty() && !m_frames.back().listing && !m_frames.back().key.has_value();
}

void value_builder::charge_node()
{
    if(m_counters.yaml_nodes >= m_limits.yaml_nodes)
        throw exhausted("auxiliary node");
    ++m_counters.yaml_nodes;
}

// Depth is a high-water mark rather than a running total, the way the evaluator's own
// session charges it.
void value_builder::admit_depth(std::size_t depth)
{
    if(depth > m_limits.yaml_depth)
        throw exhausted("auxiliary depth");
    if(depth > m_counters.yaml_depth)
        m_counters.yaml_depth = depth;
}

void value_builder::deliver(value produced, const YAML::Mark &mark)
{
    if(m_frames.empty())
    {
        if(m_root)
            throw stopped("more than one root", mark, yaml_failure::refused);
        m_root = std::move(produced);
        return;
    }
    place(m_frames.back(), std::move(produced));
}

void value_builder::place(frame &top, value produced)
{
    if(top.listing)
    {
        top.items.push_back(std::move(produced));
        return;
    }
    top.entries.emplace_back(std::move(*top.key), std::move(produced));
    top.key.reset();
}

void value_builder::take_key(const std::string &text, const std::string &tag,
                             const YAML::Mark &mark)
{
    const std::optional<std::string> name = resolved(text, tag, mark).text();
    if(!name)
        throw stopped("a non-string mapping key", mark, yaml_failure::unsupported);
    if(*name == "<<")
        throw stopped("a merge key", mark, yaml_failure::unsupported);
    frame &top = m_frames.back();
    if(!top.seen.insert(*name).second)
        throw stopped("a duplicate key '" + *name + '\'', mark, yaml_failure::refused);
    top.key = *name;
}

value value_builder::resolved(const std::string &text, const std::string &tag,
                              const YAML::Mark &mark)
{
    const scalar_result made = resolve_scalar(text, tag);
    if(!made.resolved)
        throw stopped(made.refusal, mark, made.failure);
    return *made.resolved;
}

parse_stopped value_builder::stopped(const std::string &construct, const YAML::Mark &mark,
                                     yaml_failure why)
{
    const diagnostic_code code = why == yaml_failure::unsupported
                                     ? diagnostic_code::unsupported_expression
                                     : diagnostic_code::expression_error;
    m_log.log(level::error, code, m_at,
              "the auxiliary document holds " + construct + at_mark(mark));
    return parse_stopped{ why };
}

parse_stopped value_builder::exhausted(std::string_view axis)
{
    m_log.log(level::error, diagnostic_code::expansion_budget_exceeded, m_at,
              "the evaluator's " + std::string(axis)
                  + " ceiling was reached; this load cannot continue");
    return parse_stopped{ yaml_failure::exhausted };
}

}

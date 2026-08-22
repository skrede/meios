#include "value_builder.h"
#include "scalar_resolver.h"

#include "meios/xacro/value.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <yaml-cpp/mark.h>

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

bool value_builder::expecting_key() const
{
    return !m_frames.empty() && !m_frames.back().listing && !m_frames.back().merging
        && !m_frames.back().key.has_value();
}

void value_builder::exercised(evaluator_construct one)
{
    m_counters.constructs.mark(one);
}

void value_builder::charge_node()
{
    if(m_counters.yaml_nodes >= m_limits.yaml_nodes)
        throw exhausted("auxiliary node");
    ++m_counters.yaml_nodes;
}

// The charge is the count of nodes the alias denotes, not the one event that resolved it: the
// node ceiling already charges events, and a document whose aliases denote hundreds of millions
// of nodes produces only a few dozen of them.
void value_builder::charge_alias(std::size_t denoted)
{
    if(denoted > m_limits.alias_expansion - m_counters.alias_expansion)
        throw exhausted("alias expansion");
    m_counters.alias_expansion += denoted;
}

// A key can carry an anchor as readily as a value can, so the table is filled from both paths;
// what neither may do is fill it before the node is finished, since an anchor absent from the
// table is exactly how an alias to an unfinished node is recognized.
void value_builder::remember(YAML::anchor_t anchor, const value &held, std::size_t denoted)
{
    if(anchor != YAML::NullAnchor)
        m_anchors.push_back(std::pair{ anchor, anchored{ held, denoted } });
}

// Absence is the incomplete-anchor case rather than an error of lookup: an alias to an anchor
// the parser never saw defined does not reach this builder at all.
const value_builder::anchored *value_builder::anchor_at(YAML::anchor_t anchor) const
{
    for(const std::pair<YAML::anchor_t, anchored> &one : m_anchors)
    {
        if(one.first == anchor)
            return &one.second;
    }
    return nullptr;
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

// Every value this builder produces passes through here, so marking the origin at this one
// seam marks the whole tree: a child later extracted out of a mapping or a sequence carries
// the mark by construction rather than by an extraction site remembering to apply it.
void value_builder::deliver(value produced, std::size_t denoted, YAML::anchor_t anchor,
                            const YAML::Mark &mark)
{
    value marked = produced.with_yaml_origin();
    remember(anchor, marked, denoted);
    if(m_frames.empty())
    {
        if(m_root)
            throw stopped("more than one root", mark, yaml_failure::refused);
        m_root = std::move(marked);
        return;
    }
    m_frames.back().denoted += denoted;
    if(m_frames.back().merging)
        return merge_into(m_frames.back(), marked, mark);
    place(m_frames.back(), std::move(marked));
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

value value_builder::resolved(const std::string &text, const std::string &tag,
                              const YAML::Mark &mark)
{
    const scalar_result made = resolve_scalar(text, tag);
    if(!made.resolved)
        throw stopped(made.refusal, mark, made.failure);
    // The two non-specific tags are how a scalar says it carries no tag at all, so what is left
    // here is the closed unit-tag table having converted the quantity.
    if(tag != "?" && tag != "!")
        exercised(evaluator_construct::unit_tag);
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

#include "value_builder.h"

#include "meios/xacro/value.h"

#include "meios/xacro/detail/value_key.h"

#include <yaml-cpp/mark.h>
#include <yaml-cpp/anchor.h>
#include <yaml-cpp/emitterstyle.h>

#include <string>
#include <cstddef>
#include <utility>
#include <optional>

namespace meios::detail
{

value_builder::value_builder(const evaluator_limits &ceilings, evaluator_counters &tally,
                             log_sink &sink, const source_location &origin)
    : m_log(sink), m_at(origin), m_root(), m_frames(), m_counters(tally), m_limits(ceilings),
      m_anchors()
{
}

void value_builder::OnDocumentStart(const YAML::Mark &) {}

void value_builder::OnDocumentEnd() {}

void value_builder::OnNull(const YAML::Mark &mark, YAML::anchor_t anchor)
{
    charge_node();
    if(!expecting_key())
        return deliver(value{}, 1, anchor, mark);
    remember(anchor, value{}.with_yaml_origin(), 1);
    admit_key(scalar_key{});
}

void value_builder::OnAlias(const YAML::Mark &mark, YAML::anchor_t anchor)
{
    charge_node();
    const anchored *source = anchor_at(anchor);
    if(source == nullptr)
        throw stopped("an alias to an anchor that is not yet complete", mark,
                      yaml_failure::refused);
    charge_alias(source->denoted);
    share(*source, mark);
}

void value_builder::OnScalar(const YAML::Mark &mark, const std::string &tag, YAML::anchor_t anchor,
                             const std::string &text)
{
    charge_node();
    if(expecting_key())
        return take_key(text, tag, anchor, mark);
    deliver(resolved(text, tag, mark), 1, anchor, mark);
}

void value_builder::OnSequenceStart(const YAML::Mark &mark, const std::string &tag,
                                    YAML::anchor_t anchor, YAML::EmitterStyle::value)
{
    charge_node();
    admit_depth(m_frames.size() + 1);
    if(expecting_key())
        throw stopped("a sequence as a mapping key", mark, yaml_failure::refused);
    if(tag != "?" && tag != "!")
        throw stopped("an unsupported tag '" + tag + '\'', mark, yaml_failure::refused);
    m_frames.push_back(frame{ .listing = true, .at = mark, .anchor = anchor });
}

void value_builder::OnSequenceEnd()
{
    frame done = std::move(m_frames.back());
    m_frames.pop_back();
    deliver(value::make_sequence(std::move(done.items)), done.denoted + 1, done.anchor, done.at);
}

void value_builder::OnMapStart(const YAML::Mark &mark, const std::string &tag,
                               YAML::anchor_t anchor, YAML::EmitterStyle::value)
{
    charge_node();
    admit_depth(m_frames.size() + 1);
    if(expecting_key())
        throw stopped("a mapping as a mapping key", mark, yaml_failure::refused);
    if(tag != "?" && tag != "!")
        throw stopped("an unsupported tag '" + tag + '\'', mark, yaml_failure::refused);
    m_frames.push_back(frame{ .listing = false, .at = mark, .anchor = anchor });
}

// Merged entries lead and the entries the document wrote follow, which is all the precedence
// this builder states: the mapping constructor's first-position, last-value rule turns that
// order into the resolution upstream produces.
void value_builder::OnMapEnd()
{
    frame done = std::move(m_frames.back());
    m_frames.pop_back();
    done.merged.insert(done.merged.end(), done.entries.begin(), done.entries.end());
    deliver(value::make_mapping(std::move(done.merged)), done.denoted + 1, done.anchor, done.at);
}

// An empty document raises no event, so the null it stands for is made here rather than
// delivered; it is still the value that parse produced and carries the same origin.
value value_builder::result() const
{
    return m_root.value_or(value{}.with_yaml_origin());
}

}

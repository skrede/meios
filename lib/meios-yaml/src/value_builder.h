#ifndef HPP_GUARD_MEIOS_YAML_VALUE_BUILDER_H
#define HPP_GUARD_MEIOS_YAML_VALUE_BUILDER_H

#include "meios/xacro/value.h"
#include "meios/xacro/evaluator_limits.h"
#include "meios/xacro/yaml_parser_handle.h"

#include "meios/xacro/detail/value_key.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <yaml-cpp/mark.h>
#include <yaml-cpp/anchor.h>
#include <yaml-cpp/emitterstyle.h>
#include <yaml-cpp/eventhandler.h>

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail
{

// Thrown out of an event callback. The library drives the walk, so a refused charge can only
// bound the work it precedes by unwinding that walk where the refusal happens, rather than
// letting the whole document be read and discarding the result afterwards.
struct parse_stopped
{
    yaml_failure why;
};

class value_builder final : public YAML::EventHandler
{
    // An alias shares the anchored value rather than copying it, so what the alias costs is not
    // the sharing but the size of the tree it denotes; that count is computed once, here, and
    // charged again at every site that names the anchor.
    struct anchored
    {
        value held;
        std::size_t denoted;
    };

    // The end events carry no arguments, so a collection can only learn which anchor it belongs
    // to by having remembered it from its own start event.
    struct frame
    {
        bool listing;
        bool merging;
        YAML::Mark at;
        std::size_t denoted;
        YAML::anchor_t anchor;
        std::vector<value> items;
        std::optional<scalar_key> key;
        std::vector<value::entry> merged;
        std::vector<value::entry> entries;
    };

public:
    value_builder(const evaluator_limits &ceilings, evaluator_counters &tally, log_sink &sink,
                  const source_location &origin);

    void OnDocumentStart(const YAML::Mark &mark) override;
    void OnDocumentEnd() override;
    void OnNull(const YAML::Mark &mark, YAML::anchor_t anchor) override;
    void OnAlias(const YAML::Mark &mark, YAML::anchor_t anchor) override;
    void OnScalar(const YAML::Mark &mark, const std::string &tag, YAML::anchor_t anchor,
                  const std::string &text) override;
    void OnSequenceStart(const YAML::Mark &mark, const std::string &tag, YAML::anchor_t anchor,
                         YAML::EmitterStyle::value style) override;
    void OnSequenceEnd() override;
    void OnMapStart(const YAML::Mark &mark, const std::string &tag, YAML::anchor_t anchor,
                    YAML::EmitterStyle::value style) override;
    void OnMapEnd() override;

    value result() const;

private:
    log_sink &m_log;
    source_location m_at;
    std::optional<value> m_root;
    std::vector<frame> m_frames;
    evaluator_counters &m_counters;
    const evaluator_limits &m_limits;
    std::vector<std::pair<YAML::anchor_t, anchored>> m_anchors;

    bool expecting_key() const;
    void charge_node();
    void charge_alias(std::size_t denoted);
    void admit_depth(std::size_t depth);
    void place(frame &top, value produced);
    void admit_key(scalar_key key);
    void share(const anchored &source, const YAML::Mark &mark);
    void merge_into(frame &top, const value &source, const YAML::Mark &mark);
    void gather(std::vector<value::entry> &into, const value &source, const YAML::Mark &mark);
    void remember(YAML::anchor_t anchor, const value &held, std::size_t denoted);
    void deliver(value produced, std::size_t denoted, YAML::anchor_t anchor,
                 const YAML::Mark &mark);
    const anchored *anchor_at(YAML::anchor_t anchor) const;
    void take_key(const std::string &text, const std::string &tag, YAML::anchor_t anchor,
                  const YAML::Mark &mark);
    value resolved(const std::string &text, const std::string &tag, const YAML::Mark &mark);
    parse_stopped stopped(const std::string &construct, const YAML::Mark &mark, yaml_failure why);
    parse_stopped exhausted(std::string_view axis);
};

}

#endif

#ifndef HPP_GUARD_MEIOS_YAML_VALUE_BUILDER_H
#define HPP_GUARD_MEIOS_YAML_VALUE_BUILDER_H

#include "meios/xacro/value.h"
#include "meios/xacro/evaluator_limits.h"
#include "meios/xacro/yaml_parser_handle.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <yaml-cpp/mark.h>
#include <yaml-cpp/anchor.h>
#include <yaml-cpp/emitterstyle.h>
#include <yaml-cpp/eventhandler.h>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <string_view>
#include <unordered_set>

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
    struct frame
    {
        bool listing;
        YAML::Mark at;
        std::vector<value> items;
        std::optional<std::string> key;
        std::vector<value::entry> entries;
        std::unordered_set<std::string> seen;
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

    bool expecting_key() const;
    void charge_node();
    void admit_depth(std::size_t depth);
    void deliver(value produced, const YAML::Mark &mark);
    void place(frame &top, value produced);
    void take_key(const std::string &text, const std::string &tag, const YAML::Mark &mark);
    value resolved(const std::string &text, const std::string &tag, const YAML::Mark &mark);
    parse_stopped stopped(const std::string &construct, const YAML::Mark &mark, yaml_failure why);
    parse_stopped exhausted(std::string_view axis);
};

}

#endif

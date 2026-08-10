#ifndef HPP_GUARD_MEIOS_UNIT_BRANCH_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_BRANCH_PROBE_H

#include "fake_yaml_parser.h"

#include "meios/xacro/eval_session.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace probe
{

inline constexpr std::string_view bare_document = "length: 0.12\n";
inline constexpr std::string_view full_document = "radius: 0.06\nlength: 0.12\n";

// Counts what it was asked for, so a branch that is never taken can be shown to have reached
// no auxiliary document at all rather than merely to have discarded what it read.
class counting_loader final : public meios::text_resource_loader::fetcher
{
public:
    explicit counting_loader(std::size_t &tally) : m_tally(tally) {}

    std::optional<std::string> fetch(std::string_view spec, const std::filesystem::path &) override
    {
        ++m_tally;
        return std::string(spec == "full.yaml" ? full_document : bare_document);
    }

private:
    std::size_t &m_tally;
};

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
    meios::eval_failure_kind kind;
    std::string rendered;
    std::vector<std::string> messages;
    std::size_t fetches;
};

inline void install_probe(meios::eval_scope &scope, std::size_t &fetches)
{
    scope.install_text_loader(
        meios::text_resource_loader{ std::make_unique<counting_loader>(fetches) });
    scope.install_yaml_parser(fake::yaml_parser_handle());
    scope.set("bare_file", meios::value{ std::string("bare.yaml") });
    scope.set("full_file", meios::value{ std::string("full.yaml") });
}

inline meios::eval_scope probe_scope(meios::log_sink &log, std::size_t &fetches)
{
    meios::eval_scope scope;
    install_probe(scope, fetches);
    meios::core_evaluator evaluator;
    scope.set("cfg", evaluator.eval("xacro.load_yaml(bare_file)", scope, log));
    scope.set("full", evaluator.eval("xacro.load_yaml(full_file)", scope, log));
    scope.set("x", meios::detail::classify("0"));
    scope.set("y", meios::detail::classify("2"));
    scope.set("word", meios::value{ std::string("text") });
    fetches = 0;
    return scope;
}

inline outcome evaluate(std::string_view expression)
{
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    std::size_t fetches = 0;
    const meios::eval_scope scope = probe_scope(sink, fetches);
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return outcome{ evaluator.failed(), evaluator.failure_kind(),
                    meios::render_scalar(result).value_or(std::string()), heard.messages,
                    fetches };
}

inline std::string guard_document(std::string_view named)
{
    return std::string("<?xml version=\"1.0\"?>\n<robot name=\"guard\" "
                       "xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
                       "  <xacro:property name=\"cfg\" value=\"${xacro.load_yaml(")
         + std::string(named)
         + ")}\"/>\n  <link name=\"a\"><visual><geometry>"
           "<sphere radius=\"${cfg['radius'] if 'radius' in cfg else 0.05}\"/>"
           "</geometry></visual></link>\n</robot>\n";
}

inline std::string expanded(std::string_view source, meios::log_sink &log)
{
    meios::eval_scope scope;
    std::size_t fetches = 0;
    install_probe(scope, fetches);
    meios::source_stack sources;
    const meios::expected<meios::expansion, meios::expansion_error> out =
        meios::expand(source, scope, sources, "guard.urdf.xacro", meios::expansion_limits{}, log);
    return out ? out->document : std::string();
}

inline std::size_t steps_for(std::string_view expression)
{
    const meios::evaluator_limits ceilings;
    meios::detail::eval_session session(ceilings);
    const meios::eval_scope scope;
    meios::log_sink silent;
    meios::core_evaluator evaluator;
    evaluator.eval(expression, scope, silent, {}, session);
    return session.counters.steps;
}

inline std::string nested(std::size_t depth)
{
    return std::string(depth, '(') + "1" + std::string(depth, ')');
}

}

#endif

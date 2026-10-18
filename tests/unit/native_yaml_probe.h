#ifndef HPP_GUARD_MEIOS_TEST_NATIVE_YAML_PROBE_H
#define HPP_GUARD_MEIOS_TEST_NATIVE_YAML_PROBE_H

#include "meios/xacro/eval_session.h"
#include "meios/xacro/substitution_detail.h"

#include <meios/xacro.h>

#include <meios/urdf/load.h>

#include <meios/io/source_stack.h>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace native_yaml_test
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

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }
};

class fixed_loader final : public meios::text_resource_loader::fetcher
{
public:
    explicit fixed_loader(std::string text)
            : m_text(std::move(text))
    {
    }

    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override
    {
        return m_text;
    }

private:
    std::string m_text;
};

struct run
{
    bool survived;
    std::string text;
    std::vector<std::string> messages;
};

// The whole path a description takes: a name in the scope, the contained loader for the bytes,
// the installed parser for the value, and the load's own session for the ceilings.
inline run substitute(std::string_view document, const std::shared_ptr<const meios::yaml_parser_handle> &parser, const meios::evaluator_limits &ceilings, meios::eval_policy policy,
                      std::string_view reference = "${xacro.load_yaml(params_file)['a']}")
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{std::make_unique<fixed_loader>(std::string(document))});
    scope.install_yaml_parser(parser);
    scope.set("params_file", meios::value{std::string("params.yaml")});
    meios::detail::eval_session session(ceilings);
    meios::source_stack sources;
    recorder heard;
    meios::log_sink_f sink{std::ref(heard)};
    const meios::expected<meios::substitution, meios::expansion_error> out =
            meios::detail::substitute_refined("value " + std::string(reference), scope, sources, "inline.xacro", policy, {}, session, sink, {});
    return run{out.has_value(), out.has_value() ? out.value().text : std::string(), heard.messages};
}

constexpr std::string_view nested_document = "a:\n  b:\n    c: 1\n";

#ifdef MEIOS_TEST_HAS_YAML

struct probe
{
    std::optional<meios::value> parsed;
    meios::yaml_failure failure;
    std::vector<std::string> messages;
    meios::evaluator_counters counters;
};

inline probe read(std::string_view document, const meios::evaluator_limits &ceilings)
{
    const std::shared_ptr<const meios::yaml_parser_handle> parser = meios::make_yaml_parser();
    meios::evaluator_counters counters;
    recorder heard;
    meios::log_sink_f sink{std::ref(heard)};
    const meios::yaml_outcome out = (*parser)(document, ceilings, counters, sink, {});
    return probe{out.parsed, out.failure, heard.messages, counters};
}

inline probe read(std::string_view document)
{
    const meios::evaluator_limits defaults;
    return read(document, defaults);
}

inline run substitute(std::string_view document, const meios::evaluator_limits &ceilings, meios::eval_policy policy, std::string_view reference = "${xacro.load_yaml(params_file)['a']}")
{
    return substitute(document, meios::make_yaml_parser(), ceilings, policy, reference);
}

#endif

}

#endif

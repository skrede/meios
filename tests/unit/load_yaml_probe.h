#ifndef HPP_GUARD_MEIOS_UNIT_LOAD_YAML_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_LOAD_YAML_PROBE_H

#include "fake_yaml_parser.h"

#include <meios/xacro.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace load_yaml
{

constexpr std::string_view catalogue = "name: parts\nempty:\nitems:\n  - a\n  - b\n";
constexpr std::string_view arm = "arm:\n  length: 0.12\n";

// Records the specification of every document the scope was asked for, so what the loading call
// reached is read off the calls themselves rather than asserted about them.
class recording_loader final : public meios::text_resource_loader::fetcher
{
public:
    explicit recording_loader(std::vector<std::string> &seen) : m_seen(seen) {}

    std::optional<std::string> fetch(std::string_view spec, const std::filesystem::path &) override
    {
        m_seen.emplace_back(spec);
        if(spec == "catalogue.yaml")
            return std::string(catalogue);
        if(spec == "cfg/arm.yaml")
            return std::string(arm);
        return std::nullopt;
    }

private:
    std::vector<std::string> &m_seen;
};

struct heard
{
    std::vector<std::string> messages;
    std::vector<meios::diagnostic_code> codes;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &text)
    {
        (*this)(lvl, text);
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &text)
    {
        if(lvl == meios::level::error)
            codes.push_back(code);
        (*this)(lvl, text);
    }
};

struct reading
{
    bool failed;
    std::string rendered;
    std::vector<std::string> messages;
    std::vector<meios::diagnostic_code> codes;
    std::vector<std::string> loads;
};

inline bool names(const reading &got, std::string_view fragment)
{
    for(const std::string &message : got.messages)
        if(message.find(fragment) != std::string::npos)
            return true;
    return false;
}

inline reading evaluate(std::string_view expression, bool with_loader = true)
{
    std::vector<std::string> loads;
    heard sink;
    meios::log_sink_f log{ std::ref(sink) };
    meios::eval_scope scope;
    if(with_loader)
        scope.install_text_loader(
            meios::text_resource_loader{ std::make_unique<recording_loader>(loads) });
    scope.install_yaml_parser(fake::yaml_parser_handle());
    scope.set("catalogue_file", meios::value{ std::string("catalogue.yaml") });
    scope.set("stem", meios::value{ std::string("arm") });
    meios::core_evaluator evaluator;
    const meios::value read = evaluator.eval(expression, scope, log);
    const bool failed = evaluator.failed();
    const std::string text = failed ? std::string() : meios::render_scalar(read).value_or("-");
    return reading{ failed, text, sink.messages, sink.codes, loads };
}

}

#endif

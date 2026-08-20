#ifndef HPP_GUARD_MEIOS_UNIT_CONSTRUCT_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_CONSTRUCT_PROBE_H

#include <meios/xacro.h>

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

namespace construct_probe
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

// One auxiliary document reachable under any name, so a case states the bytes it needs beside
// the expression that reads them instead of in a fixture file away from both.
class one_document final : public meios::text_resource_loader::fetcher
{
public:
    explicit one_document(std::string text) : m_text(std::move(text)) {}

    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override
    {
        return m_text;
    }

private:
    std::string m_text;
};

struct load_result
{
    bool ok;
    meios::construct_set exercised;
    std::vector<std::string> messages;
};

// The spellings in vocabulary order, which turns an observation into something a case can
// compare whole: an assertion naming a set states what the load did not do as well as what
// it did.
inline std::vector<std::string_view> named(const meios::construct_set &set)
{
    std::vector<std::string_view> out;
    for(const meios::construct_spelling &one : meios::construct_spellings)
    {
        if(set.holds(one.construct))
            out.push_back(one.name);
    }
    return out;
}

// A failure reads as the spellings themselves rather than as two containers a test framework
// cannot print.
inline std::string listed(const meios::construct_set &set)
{
    std::string out;
    for(std::string_view spelling : named(set))
    {
        out += out.empty() ? "" : ", ";
        out += spelling;
    }
    return out;
}

inline std::string document_for(std::string_view expression)
{
    return std::string(R"(<robot xmlns:xacro="http://www.ros.org/wiki/xacro" name="probe">)"
                       R"(<xacro:property name="read" value="${)")
         + std::string(expression) + R"(}"/><link name="probe"/></robot>)";
}

inline load_result read(std::string_view expression, std::string_view auxiliary,
                        const std::shared_ptr<const meios::yaml_parser_handle> &parser)
{
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    meios::eval_scope scope;
    scope.install_text_loader(
        meios::text_resource_loader{ std::make_unique<one_document>(std::string(auxiliary)) });
    if(parser)
        scope.install_yaml_parser(parser);
    meios::source_stack sources;
    const meios::expected<meios::expansion, meios::expansion_error> out =
        meios::expand(document_for(expression), scope, sources, "probe.xacro",
                      meios::expansion_limits{}, sink);
    if(!out)
        return load_result{ false, meios::construct_set{}, heard.messages };
    return load_result{ true, out.value().exercised, heard.messages };
}

inline load_result read(std::string_view expression)
{
    return read(expression, std::string_view(), nullptr);
}

}

#endif

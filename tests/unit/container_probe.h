#ifndef HPP_GUARD_MEIOS_UNIT_CONTAINER_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_CONTAINER_PROBE_H

#include "container_table.h"

#include <meios/eval/python_evaluator.h>

#include <meios/xacro.h>
#include <meios/io/source_stack.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace container
{

constexpr std::string_view configuration = "limits:\n"
                                           "  shoulder:\n"
                                           "    max: 42\n"
                                           "    min: -1.5\n"
                                           "joints:\n"
                                           "  - name: alpha\n"
                                           "  - name: beta\n"
                                           "items: 7\n"
                                           "empty:\n";

struct tally
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

struct fixed_config final : meios::text_resource_loader::fetcher
{
    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override
    {
        return std::string(configuration);
    }
};

struct reading
{
    std::optional<std::string> leaf;
    std::vector<std::string> messages;
};

inline bool names(const reading &got, const std::string &fragment)
{
    for(const std::string &message : got.messages)
    {
        if(message.find(fragment) != std::string::npos)
            return true;
    }
    return false;
}

// Read the text between the reading element's own tags rather than searching the document, so an
// expected value cannot match somewhere the expression never reached.
inline std::optional<std::string> leaf_of(const std::string &document)
{
    const std::size_t open  = document.find("<l>");
    const std::size_t close = document.find("</l>", open);
    if(open == std::string::npos || close == std::string::npos)
        return std::nullopt;
    return document.substr(open + 3, close - open - 3);
}

inline std::string document_for(const row &r)
{
    std::string document =
        "<robot xmlns:xacro=\"http://ros.org/wiki/xacro\">"
        "<xacro:property name=\"config\" value=\"${xacro.load_yaml('config.yaml')}\"/>"
        "<xacro:property name=\"section\" value=\"${config['limits']}\"/>"
        "<xacro:property name=\"joint\" value=\"${section['shoulder']}\"/>";
    if(r.subject != "-")
        document += "<xacro:property name=\"subject\" value=\"" + r.subject + "\"/>";
    return document + "<l>${" + r.expr + "}</l></robot>";
}

inline reading probe(const row &r)
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<fixed_config>() });
    meios::source_stack sources;
    const auto backend = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const meios::expected<meios::expansion, meios::expansion_error> out =
        meios::expand(document_for(r), scope, sources, "robot.xacro", meios::expansion_limits{},
                      meios::eval_policy::fail, backend, sink);
    if(!out.has_value())
        return reading{ std::nullopt, std::move(counts.messages) };
    return reading{ leaf_of(out->document), std::move(counts.messages) };
}

}

#endif

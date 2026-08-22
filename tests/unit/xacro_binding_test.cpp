#include "fake_yaml_parser.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

constexpr std::string_view arm_document = "arm:\n  length: 0.12\n";

struct one_document final : meios::text_resource_loader::fetcher
{
    explicit one_document(std::vector<std::string> &seen) : m_seen(seen) {}

    std::optional<std::string> fetch(std::string_view spec, const std::filesystem::path &) override
    {
        m_seen.emplace_back(spec);
        return std::string(arm_document);
    }

private:
    std::vector<std::string> &m_seen;
};

std::string wrap(const std::string &body)
{
    return "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">" + body + "</robot>";
}

expansion_result expand_body(const std::string &body, meios::log_sink &log,
                             std::vector<std::string> *loads = nullptr)
{
    meios::source_stack sources{};
    meios::eval_scope scope;
    if(loads)
    {
        scope.install_text_loader(
            meios::text_resource_loader{ std::make_unique<one_document>(*loads) });
        scope.install_yaml_parser(fake::yaml_parser_handle());
    }
    const std::string source = wrap(body);
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, log);
}

// The truth of the bound value and its own spelling, read out of one emitted element.
std::string bound_as(const std::string &spelling)
{
    meios::log_sink silent;
    const expansion_result out = expand_body(
        "<xacro:property name=\"flag\" value=\"" + spelling
            + "\"/><link name=\"${'y' if flag else 'n'}\" v=\"${flag}\"/>",
        silent);
    REQUIRE(out.has_value());
    const std::size_t at = out->document.find("<link");
    REQUIRE(at != std::string::npos);
    return out->document.substr(at, out->document.find('>', at) + 1 - at);
}

}

TEST_CASE("a boolean spelling binds a boolean where a number spelling still binds a number",
          "[xacro][binding]")
{
    const std::pair<std::string, std::string> rows[] = {
        { "false", "<link name=\"n\" v=\"False\"/>" }, { "true", "<link name=\"y\" v=\"True\"/>" },
        { "False", "<link name=\"n\" v=\"False\"/>" }, { "True", "<link name=\"y\" v=\"True\"/>" },
        { "1", "<link name=\"y\" v=\"1\"/>" },         { "0", "<link name=\"n\" v=\"0\"/>" },
        { "0.0", "<link name=\"n\" v=\"0.0\"/>" },     { "yes", "<link name=\"y\" v=\"yes\"/>" },
        { "no", "<link name=\"y\" v=\"no\"/>" }
    };

    for(const std::pair<std::string, std::string> &row : rows)
    {
        INFO("value=\"" << row.first << '"');
        CHECK(bound_as(row.first) == row.second);
    }
}

TEST_CASE("an argument default carrying a boolean spelling reaches a macro parameter as a boolean",
          "[xacro][binding]")
{
    meios::log_sink silent;
    const expansion_result out = expand_body(
        "<xacro:arg name=\"flag\" default=\"false\"/>"
        "<xacro:macro name=\"m\" params=\"flag\">"
        "<link name=\"${'y' if flag else 'n'}\" v=\"${flag}\"/></xacro:macro>"
        "<xacro:m flag=\"$(arg flag)\"/>",
        silent);

    REQUIRE(out.has_value());
    CHECK(out->document.find("<link name=\"n\" v=\"False\"") != std::string::npos);
}

TEST_CASE("a macro call's arguments are evaluated against the caller, never against one another",
          "[xacro][binding]")
{
    meios::log_sink silent;
    const expansion_result out = expand_body(
        "<xacro:macro name=\"m\" params=\"tag other\">"
        "<link name=\"${tag}|${other}\"/></xacro:macro>"
        "<xacro:property name=\"tag\" value=\"outer\"/>"
        "<xacro:m tag=\"${tag}_inner\" other=\"${tag}\"/>",
        silent);

    REQUIRE(out.has_value());
    CHECK(out->document.find("name=\"outer_inner|outer\"") != std::string::npos);
}

TEST_CASE("an inherited default still reads the caller while a sibling argument rebinds",
          "[xacro][binding]")
{
    meios::log_sink silent;
    const expansion_result out = expand_body(
        "<xacro:macro name=\"m\" params=\"tag other:=^\">"
        "<link name=\"${tag}|${other}\"/></xacro:macro>"
        "<xacro:property name=\"tag\" value=\"outer\"/>"
        "<xacro:property name=\"other\" value=\"kept\"/>"
        "<xacro:m tag=\"${tag}_inner\"/>",
        silent);

    REQUIRE(out.has_value());
    CHECK(out->document.find("name=\"outer_inner|kept\"") != std::string::npos);
}

// The vendor spelling: the whole attribute is one span, and inside it a command span names the
// document. The span layer resolves the inner text first, so what stays is one span and the
// mapping it yields crosses into the property with its kind.
TEST_CASE("an attribute that is one span keeps its value's kind though the span carries another",
          "[xacro][binding]")
{
    std::vector<std::string> loads;
    meios::log_sink silent;
    const expansion_result out = expand_body(
        "<xacro:arg name=\"dir\" default=\"cfg\"/>"
        "<xacro:property name=\"cfg\" value=\"${xacro.load_yaml('$(arg dir)/arm.yaml')}\"/>"
        "<link name=\"l\"><geometry length=\"${cfg['arm']['length']}\"/></link>",
        silent, &loads);

    REQUIRE(out.has_value());
    CHECK(out->document.find("length=\"0.12\"") != std::string::npos);
    CHECK(loads == std::vector<std::string>{ "cfg/arm.yaml" });
}

TEST_CASE("a text a one-span attribute evaluates is classified as an authored one is",
          "[xacro][binding]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_body("<xacro:property name=\"p\" value=\"${'0' + '1'}\"/><link name=\"${p}\"/>",
                    silent);

    REQUIRE(out.has_value());
    CHECK(out->document.find("name=\"1\"") != std::string::npos);
}

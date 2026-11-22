#include "syntax_probe.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace
{

struct literal_case
{
    std::string_view spelling;
    std::string_view expected;
};

meios::expected<meios::expansion, meios::expansion_error> strictly(std::string_view expression)
{
    return syntax_probe::load_under("${" + std::string(expression) + "}", meios::eval_policy::fail);
}

}

TEST_CASE("an underscored, prefixed or pointed literal evaluates to its number", "[native][syntax]")
{
    for(const literal_case one : {literal_case{"0_0", "0"},
                                  {"00_0", "0"},
                                  {"1_000", "1000"},
                                  {"1_000_000", "1000000"},
                                  {"0x10", "16"},
                                  {"0X1_f", "31"},
                                  {"0x_1f", "31"},
                                  {"0o17", "15"},
                                  {"0O1_7", "15"},
                                  {"0b101", "5"},
                                  {"0B1_0", "2"},
                                  {"-0x10", "-16"},
                                  {"0x1F + 1", "32"},
                                  {"1_0.5", "10.5"},
                                  {".5_0", "0.5"},
                                  {"0_7.5", "7.5"},
                                  {"07.5", "7.5"},
                                  {"0_7e1", "70.0"},
                                  {"1_0e1_0", "100000000000.0"},
                                  {"1.e5", "100000.0"}})
    {
        INFO("literal: " << one.spelling);
        CHECK(probe::evaluate(one.spelling).rendered == one.expected);
    }
}

TEST_CASE("an imaginary literal refuses as unsupported, naming a complex number", "[native][syntax]")
{
    for(std::string_view literal : {"07j", "0_7j", "1_0j", "1.5j", "1e3j", "0j", ".5j"})
    {
        INFO("literal: " << literal);
        const probe::outcome ran = probe::evaluate(literal);
        CHECK(ran.failed);
        CHECK(ran.kind == meios::eval_failure_kind::unsupported);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find("complex number") != std::string::npos);
        const auto expanded = strictly(literal);
        REQUIRE_FALSE(expanded.has_value());
        CHECK(expanded.error().code == meios::diagnostic_code::unsupported_expression);
    }
}

TEST_CASE("a lenient policy leaves an imaginary literal's span as written", "[native][syntax]")
{
    for(meios::eval_policy policy : {meios::eval_policy::skip, meios::eval_policy::warn})
    {
        const auto expanded = syntax_probe::load_under("<m v=\"${07j}\"/>", policy);
        REQUIRE(expanded.has_value());
        CHECK(expanded->document.find("v=\"${07j}\"") != std::string::npos);
    }
    CHECK_FALSE(syntax_probe::loads("<m v=\"${07j}\"/>", meios::eval_policy::fail));
}

TEST_CASE("a number directly followed by a keyword still reads as the number and the keyword", "[native][syntax]")
{
    for(const literal_case one : {literal_case{"1 if True else 2", "1"},
                                  {"1if True else 2", "1"},
                                  {"1if 1else 2", "1"},
                                  {"1and 0", "0"},
                                  {"0b1and 2", "2"},
                                  {"1 if 0else 2", "2"},
                                  {"True or 1", "True"},
                                  {"not 1in cfg", "True"}})
    {
        INFO("expression: " << one.spelling);
        CHECK(probe::evaluate(one.spelling).rendered == one.expected);
    }
}

TEST_CASE("a property's prefixed or underscored text keeps its literal reading", "[native][syntax]")
{
    for(std::string_view text : {"0x10", "1_0"})
    {
        INFO("value: " << text);
        const std::string body = "<xacro:property name=\"p\" value=\"" + std::string(text) + "\"/><m v=\"${p}\"/>";
        const auto expanded    = syntax_probe::load_under(body, meios::eval_policy::fail);
        REQUIRE(expanded.has_value());
        CHECK(expanded->document.find("v=\"" + std::string(text) + "\"") != std::string::npos);
    }
    const auto leading = strictly("007");
    REQUIRE_FALSE(leading.has_value());
    CHECK(leading.error().message.find("leading zero") != std::string::npos);
}

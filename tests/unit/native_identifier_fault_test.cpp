#include "native_expression_fixture.h"

#include "meios/detail/utf8.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <utility>
#include <string_view>

using native_expression_test::evaluate;

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

struct refusal
{
    bool refused;
    meios::diagnostic_code code;
    std::string message;
};

refusal read_body(std::string_view body)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string text     = R"(<robot name="r" xmlns:xacro="http://www.ros.org/wiki/xacro">)" + std::string(body) + "</robot>";
    const expansion_result out = meios::expand(text, scope, sources, "inline.xacro", meios::expansion_limits{}, silent);
    if(out.has_value())
        return refusal{false, meios::diagnostic_code::unspecified, std::string()};
    return refusal{true, out.error().code, out.error().message};
}

refusal read_link(std::string_view expression)
{
    return read_body("<link name=\"l" + std::string(expression) + "\"/>");
}

bool well_formed(std::string_view text)
{
    for(std::size_t at = 0; at < text.size();)
    {
        const meios::detail::utf8_point one = meios::detail::decode_utf8(text, at);
        if(one.value == meios::detail::malformed_utf8)
            return false;
        at += one.length;
    }
    return true;
}

bool refuses_holding(const refusal &got, meios::diagnostic_code code, std::string_view fragment)
{
    return got.refused && got.code == code && got.message.find(fragment) != std::string::npos && well_formed(got.message);
}

bool evaluation_refuses_holding(std::string_view expression, std::string_view fragment)
{
    const native_expression_test::outcome got = evaluate(expression);
    return got.failed && got.messages.size() == 1 && got.messages.front().find(fragment) != std::string::npos && well_formed(got.messages.front());
}

}

TEST_CASE("a name that normalizes to a keyword is a name, refused as undefined", "[xacro][identifier]")
{
    REQUIRE(refuses_holding(read_link("${ｉｆ}"), meios::diagnostic_code::undefined_property, "name 'if' is not defined"));
}

TEST_CASE("a name that normalizes to one of Python's constants refuses as Python refuses it", "[xacro][identifier]")
{
    const std::pair<std::string_view, std::string_view> constants[] = {{"Ｔrue", "True"}, {"Ｆalse", "False"}, {"Ｎone", "None"}};
    for(const auto &[written, constant] : constants)
    {
        const std::string message = "identifier field can't represent '" + std::string(constant) + "' constant";
        REQUIRE(refuses_holding(read_link("${" + std::string(written) + "}"), meios::diagnostic_code::expression_error, message));
    }
    const refusal shadowed = read_body(R"(<xacro:macro name="m" params="True"><link name="l${Ｔrue}"/></xacro:macro><xacro:m True="1"/>)");
    REQUIRE(refuses_holding(shadowed, meios::diagnostic_code::expression_error, "identifier field can't represent 'True' constant"));
}

TEST_CASE("a constant spelling is reported ahead of a syntax error that comes before it", "[xacro][identifier]")
{
    for(std::string_view expression : {"${1 2 Ｔrue}", "${1 2 + Ｔrue}"})
        REQUIRE(refuses_holding(read_link(expression), meios::diagnostic_code::expression_error, "identifier field can't represent 'True' constant"));
}

TEST_CASE("a character no identifier admits refuses with Python's lexical message", "[xacro][identifier]")
{
    const std::pair<std::string_view, std::string_view> faults[] = {{"${1 € 2}", "invalid character '€' (U+20AC)"},
                                                                    {"${1\u00A0+ 2}", "invalid non-printable character U+00A0"},
                                                                    {"${１}", "invalid character '１' (U+FF11)"},
                                                                    {"${a€b}", "invalid character '€' (U+20AC)"},
                                                                    {"${(1 + ) €}", "invalid character '€' (U+20AC)"}};
    for(const auto &[expression, message] : faults)
        REQUIRE(refuses_holding(read_link(expression), meios::diagnostic_code::expression_error, message));
}

TEST_CASE("malformed UTF-8 in an expression refuses naming the byte, never a split character", "[xacro][identifier]")
{
    REQUIRE(evaluation_refuses_holding("\xCF", "invalid UTF-8 in expression at byte 0xCF"));
    REQUIRE(evaluation_refuses_holding("a\x80 + 1", "invalid UTF-8 in expression at byte 0x80"));
    REQUIRE(evaluation_refuses_holding("1 + \xE2\x82", "invalid UTF-8 in expression at byte 0xE2"));
}

TEST_CASE("a lexical refusal the reader already gave keeps its code and message", "[xacro][identifier]")
{
    const refusal quoted = read_link("${1 'x}");
    REQUIRE(quoted.refused);
    REQUIRE(quoted.code == meios::diagnostic_code::unterminated_substitution);
    REQUIRE(quoted.message == "unterminated ${ substitution span at offset 1");
    REQUIRE(evaluation_refuses_holding("1 'x", "unexpected trailing tokens in expression"));
    REQUIRE(evaluation_refuses_holding("1e + €", "unrecognized character in expression: '1e'"));
}

TEST_CASE("a number followed by a readable name is a syntax refusal, not a read of the name", "[xacro][identifier]")
{
    for(std::string_view name : {"π", "ﬁ"})
        REQUIRE(refuses_holding(read_link("${1" + std::string(name) + "}"), meios::diagnostic_code::expression_error, "unexpected '" + std::string(name) + "' in expression"));
}

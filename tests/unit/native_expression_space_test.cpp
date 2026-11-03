#include "eval_policy_fixture.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace
{

struct reading
{
    bool refused;
    meios::diagnostic_code code;
    std::string text;
};

constexpr std::string_view bindings = R"(<xacro:arg name="a" default="2"/><xacro:property name="p" value="7"/>)";

// The characters outside ASCII that Python 3.14's str.isspace() holds.
constexpr std::string_view spaces[] = {"\u0085", "\u00A0", "\u1680", "\u2000", "\u2001", "\u2002", "\u2003", "\u2004", "\u2005", "\u2006",
                                       "\u2007", "\u2008", "\u2009", "\u200A", "\u2028", "\u2029", "\u202F", "\u205F", "\u3000"};

reading read_body(std::string_view body)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string text     = R"(<robot name="r" xmlns:xacro="http://www.ros.org/wiki/xacro">)" + std::string(bindings) + std::string(body) + "</robot>";
    const expansion_result out = meios::expand(text, scope, sources, "inline.xacro", meios::expansion_limits{}, silent);
    if(out.has_value())
        return reading{false, meios::diagnostic_code::unspecified, out->document};
    return reading{true, out.error().code, out.error().message};
}

reading read_link(std::string_view name)
{
    return read_body("<link name=\"" + std::string(name) + "\"/>");
}

bool renders(const reading &got, std::string_view name)
{
    return !got.refused && got.text.find("name=\"" + std::string(name) + "\"") != std::string::npos;
}

bool refuses_holding(const reading &got, meios::diagnostic_code code, std::string_view fragment)
{
    return got.refused && got.code == code && got.text.find(fragment) != std::string::npos;
}

}

TEST_CASE("Python's whitespace at either end of an expression is stripped before the expression is read", "[xacro][expression]")
{
    for(const std::string_view read : {"l${\u00A0p}", "l${p\u3000}", "l${p\u2003}", "l${\u00A0p\u3000}"})
        REQUIRE(renders(read_link(read), "l7"));
    for(const std::string_view space : spaces)
    {
        const std::string edge(space);
        INFO(edge);
        REQUIRE(renders(read_link("l${" + edge + "p}"), "l7"));
        REQUIRE(renders(read_link("l${p" + edge + "}"), "l7"));
        REQUIRE(renders(read_link("l${" + edge + "p" + edge + "}"), "l7"));
    }
}

TEST_CASE("a tab or a line feed at both ends of an expression still reads the name", "[xacro][expression]")
{
    REQUIRE(renders(read_link("l${&#9;p&#9;}"), "l7"));
    REQUIRE(renders(read_link("l${&#10;p&#10;}"), "l7"));
}

TEST_CASE("an expression is stripped after its own substitutions, wherever the expression is read", "[xacro][expression]")
{
    REQUIRE(renders(read_link("l${$(arg a)\u00A0}"), "l2"));
    REQUIRE(renders(read_link("${\u00A0p}"), "7"));
    REQUIRE(renders(read_body("<xacro:property name=\"q\" value=\"${\u00A0p}\"/><link name=\"l${q}\"/>"), "l7"));
    REQUIRE(renders(read_body("<xacro:if value=\"${\u00A0p == 7}\"><link name=\"branch\"/></xacro:if>"), "branch"));
}

TEST_CASE("whitespace inside an expression refuses, and an expression of only whitespace refuses as the empty one", "[xacro][expression]")
{
    REQUIRE(refuses_holding(read_link("l${1\u00A0+ 2}"), meios::diagnostic_code::expression_error, "invalid non-printable character U+00A0"));
    const reading spaced = read_link("l${\u00A0}");
    const reading blank  = read_link("l${ }");
    REQUIRE(spaced.refused);
    REQUIRE(blank.refused);
    REQUIRE(spaced.code == blank.code);
    REQUIRE(spaced.text == blank.text);
}

TEST_CASE("the evaluation command's body is read as written, so whitespace at either end refuses", "[xacro][expression]")
{
    for(const std::string_view read : {"l$(eval \u00A01)", "l$(eval 1\u00A0)"})
        REQUIRE(refuses_holding(read_link(read), meios::diagnostic_code::expression_error, "invalid non-printable character U+00A0"));
}

TEST_CASE("a lenient policy renders an expression with whitespace at its edge and still fails one with whitespace inside", "[xacro][expression]")
{
    for(const meios::eval_policy policy : {meios::eval_policy::skip, meios::eval_policy::warn})
    {
        REQUIRE(leaves(run(span_document("<xacro:property name=\"p\" value=\"7\"/>l${\u00A0p}"), policy), "<l>l7</l>"));
        REQUIRE(leaves(run(conditional_document("${\u00A01 == 1}"), policy), "<l/>"));
    }
    const outcome inside = run(span_document("l${1\u00A0+ 2}"), meios::eval_policy::skip);
    REQUIRE_FALSE(inside.expanded);
    REQUIRE(inside.expanded.error().code == meios::diagnostic_code::expression_error);
}

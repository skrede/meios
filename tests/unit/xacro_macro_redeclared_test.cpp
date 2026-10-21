#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result call_with(const std::string &spec, const std::string &attrs, const std::string &children, const std::string &body)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string source = R"(<robot name="r" xmlns:xacro="http://www.ros.org/wiki/xacro"><xacro:property name="x" value="vx"/>)"
                               R"(<xacro:macro name="m" params=")"
                             + spec + R"(">)" + body + "</xacro:macro><xacro:m " + attrs + ">" + children + "</xacro:m></robot>";
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, silent);
}

std::string rendered(const std::string &links)
{
    return R"(<?xml version="1.0"?><robot name="r">)" + links + "</robot>";
}

const std::string link_a = R"(<link name="${a}"/>)";

}

TEST_CASE("a parameter declared without a default and again with one binds the default at both", "[xacro][structural][macro]")
{
    const expansion_result out = call_with("a a='q r' **c", "", "<visual/>", link_a + link_a + R"(<xacro:insert_block name="c"/>)");
    REQUIRE(out.has_value());
    REQUIRE(out->document == rendered(R"(<link name="q r"/><link name="q r"/>)"));
}

TEST_CASE("a parameter declared again with an inheriting fallback binds the fallback at both", "[xacro][structural][macro]")
{
    const expansion_result out = call_with("b=${x} c  c:=^|${1 + 2}", "", "", R"(<link name="${b}"/><link name="${c}"/><link name="${c}"/>)");
    REQUIRE(out.has_value());
    REQUIRE(out->document == rendered(R"(<link name="vx"/><link name="3"/><link name="3"/>)"));
}

TEST_CASE("a default written before a declaration without one binds there, over the caller's value", "[xacro][structural][macro]")
{
    const expansion_result out = call_with("a:=1 a", R"(a="5")", "", link_a);
    REQUIRE(out.has_value());
    REQUIRE(out->document == rendered(R"(<link name="1"/>)"));
}

TEST_CASE("an empty default written last leaves the name with no default", "[xacro][structural][macro]")
{
    for(const char *spec : { "a:=1 a:=", "a=1 a=" })
    {
        const expansion_result out = call_with(spec, R"(a="5")", "", link_a);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    }
}

TEST_CASE("a default another declaration of its name replaced is still evaluated at the call", "[xacro][structural][macro]")
{
    const expansion_result out = call_with("a:=${cp} a:=2", "", "", link_a);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::undefined_property);
}

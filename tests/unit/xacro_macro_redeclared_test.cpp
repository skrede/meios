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

namespace
{

expansion_result expand_inner(const std::string &inner)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string source = R"(<robot name="r" xmlns:xacro="http://www.ros.org/wiki/xacro"><xacro:property name="x" value="vx"/>)" + inner + "</robot>";
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, silent);
}

const std::string twice_a = R"(<xacro:macro name="m" params="a a:=2"><link name="i${a}"/></xacro:macro><xacro:m a="1"/>)";
const std::string parent_from_nested = R"(<xacro:macro name="inner"><xacro:property name="p" value="9" scope="parent"/></xacro:macro>)"
                                       R"(<xacro:macro name="outer"><xacro:inner/><link name="in${p}"/></xacro:macro><xacro:outer/>)";
std::string nested_writer(const std::string &scope)
{
    return R"(<xacro:macro name="w"><xacro:property name="z" value="3" scope=")" + scope + R"("/><link name="w${z}"/></xacro:macro>)"
           R"(<xacro:macro name="outer"><xacro:w/></xacro:macro><xacro:outer/>)";
}

}

TEST_CASE("a parameter declared twice is bound in the call's table and gone after the call", "[xacro][structural][macro]")
{
    const expansion_result inside = expand_inner(twice_a);
    REQUIRE(inside.has_value());
    REQUIRE(inside->document == rendered(R"(<link name="i2"/>)"));
    const expansion_result after = expand_inner(twice_a + R"(<link name="o${a}"/>)");
    REQUIRE_FALSE(after.has_value());
    REQUIRE(after.error().code == meios::diagnostic_code::undefined_property);
}

TEST_CASE("a parameter declared twice over an outer property leaves the outer value readable after the call", "[xacro][structural][macro]")
{
    const expansion_result out = expand_inner(R"(<xacro:macro name="m" params="x x:=2"><link name="i${x}"/></xacro:macro><xacro:m x="1"/><link name="o${x}"/>)");
    REQUIRE(out.has_value());
    REQUIRE(out->document == rendered(R"(<link name="i2"/><link name="ovx"/>)"));
}

TEST_CASE("a parent-scope property from a nested call lands in the caller's table and goes with it", "[xacro][structural][macro]")
{
    const expansion_result inside = expand_inner(parent_from_nested);
    REQUIRE(inside.has_value());
    REQUIRE(inside->document == rendered(R"(<link name="in9"/>)"));
    const expansion_result after = expand_inner(parent_from_nested + R"(<link name="o${p}"/>)");
    REQUIRE_FALSE(after.has_value());
    REQUIRE(after.error().code == meios::diagnostic_code::undefined_property);
}

TEST_CASE("a global-scope property from a nested call persists at document level", "[xacro][structural][macro]")
{
    const expansion_result out = expand_inner(nested_writer("global") + R"(<link name="o${z}"/>)");
    REQUIRE(out.has_value());
    REQUIRE(out->document == rendered(R"(<link name="w3"/><link name="o3"/>)"));
}

TEST_CASE("a property of an unknown scope stays in the writing call's table", "[xacro][structural][macro]")
{
    const expansion_result inside = expand_inner(nested_writer("foo"));
    REQUIRE(inside.has_value());
    REQUIRE(inside->document == rendered(R"(<link name="w3"/>)"));
    const expansion_result after = expand_inner(nested_writer("foo") + R"(<link name="o${z}"/>)");
    REQUIRE_FALSE(after.has_value());
    REQUIRE(after.error().code == meios::diagnostic_code::undefined_property);
}

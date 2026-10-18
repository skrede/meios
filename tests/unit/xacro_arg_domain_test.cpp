#include "xacro_arg_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace
{

std::string document_of(std::string_view body)
{
    return "<?xml version=\"1.0\"?>\n<robot xmlns:xacro=\"http://wiki.ros.org/xacro\" name=\"probe\">\n" + std::string(body) + "\n</robot>";
}

std::string rendered(std::string_view body)
{
    return flattened(document_of(body));
}

expansion_result refused(std::string_view body)
{
    recorder log;
    expansion_result out = run(document_of(body), log);
    REQUIRE_FALSE(out.has_value());
    return out;
}

void require_names_argument_command(std::string_view name, std::string_view body)
{
    const expansion_result out = refused(body);
    REQUIRE(out.error().code == meios::diagnostic_code::undefined_property);
    REQUIRE(out.error().message.find("$(arg " + std::string(name) + ")") != std::string::npos);
}
}

TEST_CASE("an argument's boolean spelling renders as written in an attribute and in element text",
          "[xacro][arg][domain]")
{
    const std::string out = rendered(R"XML(  <xacro:arg name="flag" default="false"/>
  <link name="base_$(arg flag)"/>
  <self_collide>$(arg flag)</self_collide>)XML");

    REQUIRE(out == meios::canonical_xml(R"XML(<robot name="probe">
  <link name="base_false"/>
  <self_collide>false</self_collide>
</robot>)XML"));
}

TEST_CASE("a capitalized default renders with its capital rather than a lowered spelling",
          "[xacro][arg][domain]")
{
    const std::string out = rendered(R"XML(  <xacro:arg name="flag" default="True"/>
  <link name="base_$(arg flag)"/>
  <self_collide>$(arg flag)</self_collide>)XML");

    REQUIRE(out == meios::canonical_xml(R"XML(<robot name="probe">
  <link name="base_True"/>
  <self_collide>True</self_collide>
</robot>)XML"));
}

TEST_CASE("an argument's numeric spelling renders its trailing zero rather than a re-rendered real",
          "[xacro][arg][domain]")
{
    const std::string out = rendered(R"XML(  <xacro:arg name="r" default="0.10"/>
  <link name="l" r="$(arg r)"/>)XML");

    REQUIRE(out == meios::canonical_xml(
                       R"XML(<robot name="probe"><link name="l" r="0.10"/></robot>)XML"));
}

// The forward scan is an acceptance of this implementation's own: a use standing above its own
// declaration resolves here, where the reference refuses the whole document with an
// undefined-argument diagnostic. What is asserted is this project's render, not an agreement.
TEST_CASE("a use above its own declaration renders the same spelling the declaration seeds",
          "[xacro][arg][domain]")
{
    const std::string out = rendered(R"XML(  <link name="above_$(arg flag)"/>
  <self_collide>$(arg flag)</self_collide>
  <xacro:arg name="flag" default="false"/>)XML");

    REQUIRE(out == meios::canonical_xml(R"XML(<robot name="probe">
  <link name="above_false"/>
  <self_collide>false</self_collide>
</robot>)XML"));
}

// A conditional's test is read from the substituted text and answered on true/false/1/0 before
// anything is evaluated, so a boolean and the spelling of one reach the same branch.
TEST_CASE("a conditional testing an argument through the substitution command takes its branch",
          "[xacro][arg][domain]")
{
    const std::string out = rendered(R"XML(  <xacro:arg name="flag" default="false"/>
  <xacro:if value="$(arg flag)"><link name="if_fired"/></xacro:if>
  <xacro:unless value="$(arg flag)"><link name="unless_fired"/></xacro:unless>)XML");

    REQUIRE(out == meios::canonical_xml(
                       R"XML(<robot name="probe"><link name="unless_fired"/></robot>)XML"));
}

// An expression reads properties alone, as the reference's does: an argument is reached only
// through the substitution commands, and an expression naming one refuses with that spelling.
TEST_CASE("a conditional testing an argument by its bare name refuses and names the argument command", "[xacro][arg][domain]")
{
    require_names_argument_command("flag", R"XML(  <xacro:arg name="flag" default="false"/>
  <xacro:if value="${flag}"><link name="if_fired"/></xacro:if>)XML");
}

TEST_CASE("an argument read by its bare name refuses and names the argument command", "[xacro][arg][domain]")
{
    require_names_argument_command("flag", R"XML(  <xacro:arg name="flag" default="false"/>
  <link name="bare_${flag}"/>)XML");
}

TEST_CASE("a bare name computing on an argument refuses as the plain read does", "[xacro][arg][domain]")
{
    require_names_argument_command("flag", R"XML(  <xacro:arg name="flag" default="false"/>
  <link name="neg_${not flag}"/>)XML");
}

TEST_CASE("a dotted read whose head is an argument refuses and names the argument command", "[xacro][arg][domain]")
{
    require_names_argument_command("flag", R"XML(  <xacro:arg name="flag" default="false"/>
  <link name="dot_${flag.x}"/>)XML");
}

TEST_CASE("the reference's argument helper refuses and names the one helper either evaluator exposes", "[xacro][arg][domain]")
{
    const expansion_result out = refused(R"XML(  <xacro:arg name="p" default="A"/>
  <link name="${xacro.arg('p')}arm"/>)XML");

    REQUIRE(out.error().code == meios::diagnostic_code::unsupported_expression);
    REQUIRE(out.error().message.find("xacro.load_yaml") != std::string::npos);
    REQUIRE(out.error().message.find("$(arg") != std::string::npos);
    CHECK(out.error().message.find("eval-python") == std::string::npos);
}

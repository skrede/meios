#include "xacro_arg_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace
{

std::string rendered(std::string_view body)
{
    const std::string doc =
        "<?xml version=\"1.0\"?>\n<robot xmlns:xacro=\"http://wiki.ros.org/xacro\" name=\"probe\">\n"
        + std::string(body) + "\n</robot>";
    return flattened(doc);
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
// anything is evaluated, so a boolean and the spelling of one reach the same branch. These two
// cases guard the permissive direction against a later change; neither discriminates how an
// argument is bound.
TEST_CASE("a conditional testing an argument through the substitution command takes its branch",
          "[xacro][arg][domain]")
{
    const std::string out = rendered(R"XML(  <xacro:arg name="flag" default="false"/>
  <xacro:if value="$(arg flag)"><link name="if_fired"/></xacro:if>
  <xacro:unless value="$(arg flag)"><link name="unless_fired"/></xacro:unless>)XML");

    REQUIRE(out == meios::canonical_xml(
                       R"XML(<robot name="probe"><link name="unless_fired"/></robot>)XML"));
}

TEST_CASE("a conditional testing the same argument by its bare name takes the same branch",
          "[xacro][arg][domain]")
{
    const std::string out = rendered(R"XML(  <xacro:arg name="flag" default="false"/>
  <xacro:if value="${flag}"><link name="if_fired"/></xacro:if>
  <xacro:unless value="${flag}"><link name="unless_fired"/></xacro:unless>)XML");

    REQUIRE(out == meios::canonical_xml(
                       R"XML(<robot name="probe"><link name="unless_fired"/></robot>)XML"));
}

// An argument and a property share one symbol table here, where the reference keeps two and
// refuses a bare name that names an argument. This pins what such a name resolves to in this
// implementation: the characters that were written.
TEST_CASE("an argument read by its bare name renders the characters that were written",
          "[xacro][arg][domain]")
{
    const std::string out = rendered(R"XML(  <xacro:arg name="flag" default="false"/>
  <link name="bare_${flag}" v="${flag}"/>)XML");

    REQUIRE(out == meios::canonical_xml(
                       R"XML(<robot name="probe"><link name="bare_false" v="false"/></robot>)XML"));
}

// The same one table, read by a spelling that computes rather than renders: what the name binds is
// text, so its truth is a non-empty string's. The reference refuses this spelling as it refuses the
// plain one, so this pins a behavior only this implementation has.
TEST_CASE("a bare name computing on an argument computes on the text that was written",
          "[xacro][arg][domain]")
{
    const std::string out = rendered(R"XML(  <xacro:arg name="flag" default="false"/>
  <link name="neg_${not flag}"/>)XML");

    REQUIRE(out == meios::canonical_xml(
                       R"XML(<robot name="probe"><link name="neg_False"/></robot>)XML"));
}

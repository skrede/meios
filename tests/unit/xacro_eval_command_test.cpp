#include "xacro_arg_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string_view>

namespace
{

std::string probe(std::string_view fixture)
{
    std::ifstream in(std::filesystem::path{MEIOS_XACRO_PROBE_DIR} / (std::string(fixture) + ".xacro"), std::ios::binary);
    REQUIRE(in);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::string document_of(std::string_view body)
{
    return "<robot xmlns:xacro=\"http://wiki.ros.org/xacro\" name=\"probe\">" + std::string(body) + "</robot>";
}

std::string single_link(std::string_view name)
{
    return meios::canonical_xml("<robot name=\"probe\"><link name=\"" + std::string(name) + "\"/></robot>");
}

meios::expansion_error refusal(std::string_view source)
{
    recorder log;
    const expansion_result out = run(source, log);
    REQUIRE_FALSE(out.has_value());
    return out.error();
}

}

TEST_CASE("the evaluation command reads an argument holding text, as the reference does", "[xacro][eval_command]")
{
    REQUIRE(flattened(probe("eval_string_argument")) == single_link("Aarm"));
}

TEST_CASE("the evaluation command compares an argument holding text inside a conditional", "[xacro][eval_command]")
{
    REQUIRE(flattened(probe("eval_string_condition")) == single_link("yes_x"));
}

TEST_CASE("the evaluation command reads the argument where a property shares its name", "[xacro][eval_command]")
{
    REQUIRE(flattened(probe("eval_reads_argument_over_property")) == single_link("4"));
}

TEST_CASE("the evaluation command types an argument by the reference's own conversion", "[xacro][eval_command]")
{
    REQUIRE(flattened(probe("eval_argument_typing")) == single_link("1.5_True_1e3_1000_8"));
}

TEST_CASE("the evaluation command resolves the reference's own names before an argument", "[xacro][eval_command]")
{
    REQUIRE(flattened(probe("eval_shadowed_names")) == single_link("2.718281828459045_True_L_6.283185307179586"));
}

TEST_CASE("the evaluation command reads no property, and its refusal names no argument command", "[xacro][eval_command]")
{
    const meios::expansion_error error = refusal(probe("eval_reads_no_property"));
    REQUIRE(error.code == meios::diagnostic_code::undefined_property);
    CHECK(error.message.find("$(arg") == std::string::npos);
}

TEST_CASE("a substitution nested inside the evaluation command refuses, as the reference's pattern ends the command there", "[xacro][eval_command]")
{
    REQUIRE(refusal(probe("eval_nested_command")).code == meios::diagnostic_code::unterminated_substitution);
}

TEST_CASE("the evaluation command ends at its first closing parenthesis", "[xacro][eval_command]")
{
    REQUIRE(refusal(probe("eval_parenthesis_ends_command")).code == meios::diagnostic_code::expression_error);
    REQUIRE(flattened(document_of(R"XML(<xacro:arg name="p" default="A"/><link name="$(eval '(' + p)"/>)XML")) == single_link("(A"));
}

TEST_CASE("an argument the evaluation command reads and cannot carry refuses by name", "[xacro][eval_command]")
{
    const meios::expansion_error error = refusal(document_of(R"XML(<xacro:arg name="q" default="99999999999999999999"/><link name="$(eval q + 1)"/>)XML"));
    REQUIRE(error.code == meios::diagnostic_code::expression_error);
    REQUIRE(error.message.find("'q'") != std::string::npos);
}

TEST_CASE("an argument the evaluation command does not read is never converted", "[xacro][eval_command]")
{
    REQUIRE(flattened(document_of(R"XML(<xacro:arg name="q" default="99999999999999999999"/><link name="$(eval 1 + 1)"/>)XML")) == single_link("2"));
}

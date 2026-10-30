#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <filesystem>
#include <string_view>

namespace
{

using include_tree::expansion_result;

expansion_result expand_body(std::string_view body)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    return include_tree::expand_text(include_tree::robot(body), "inline.xacro", sources, silent);
}

std::string removes(std::string_view name, std::string_view flag = "true")
{
    return "<xacro:property name=\"" + std::string(name) + "\" remove=\"" + std::string(flag) + "\"/>";
}

bool refuses_with(const expansion_result &out, meios::diagnostic_code code, std::string_view naming)
{
    return !out.has_value() && out.error().code == code && out.error().message.find(naming) != std::string::npos;
}

}

TEST_CASE("a removed property is no longer defined", "[xacro][scope][property]")
{
    const expansion_result out = expand_body("<xacro:property name=\"p\" value=\"1\"/>" + removes("p") + "<link name=\"l${p}\"/>");
    REQUIRE(refuses_with(out, meios::diagnostic_code::undefined_property, "p"));
}

TEST_CASE("a removal inside a macro body erases the document's binding", "[xacro][scope][property]")
{
    const expansion_result out = expand_body("<xacro:property name=\"b\" value=\"1\"/><xacro:macro name=\"m\">" + removes("b")
                                             + "</xacro:macro><xacro:m/><link name=\"l${b}\"/>");
    REQUIRE(refuses_with(out, meios::diagnostic_code::undefined_property, "b"));
}

TEST_CASE("a removal inside a macro body erases the call's parameter", "[xacro][scope][property]")
{
    const expansion_result out = expand_body("<xacro:macro name=\"m\" params=\"b\">" + removes("b")
                                             + "<link name=\"l${b}\"/></xacro:macro><xacro:m b=\"1\"/>");
    REQUIRE(refuses_with(out, meios::diagnostic_code::undefined_property, "b"));
}

TEST_CASE("a property writing a value and a removal together refuses", "[xacro][scope][property]")
{
    const expansion_result out = expand_body("<xacro:property name=\"p\" value=\"1\" remove=\"true\"/><link name=\"l\"/>");
    REQUIRE(refuses_with(out, meios::diagnostic_code::xacro_structural_error,
                         "Property attributes default, value, and remove are mutually exclusive: p"));
}

TEST_CASE("a removal is read with the conditionals' boolean rule, and a false one sits beside a value", "[xacro][scope][property]")
{
    const std::string bound = "<xacro:property name=\"p\" value=\"1\"/>";
    const std::string read = "<link name=\"l${p}\"/>";
    for(const std::string_view flag : { "1", "${True}" })
        REQUIRE(refuses_with(expand_body(bound + removes("p", flag) + read), meios::diagnostic_code::undefined_property, "p"));
    for(const std::string_view flag : { "0", "false" })
        REQUIRE(include_tree::renders_link(
            expand_body(bound + "<xacro:property name=\"p\" value=\"2\" remove=\"" + std::string(flag) + "\"/>" + read), "l2"));
    REQUIRE(refuses_with(expand_body(bound + removes("p", "yes") + read), meios::diagnostic_code::xacro_structural_error,
                         "Xacro conditional"));
}

TEST_CASE("removing an include namespace's name leaves its macros unreachable", "[xacro][scope][property][namespace]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot("<xacro:macro name=\"m\" params=\"\"><link name=\"nm\"/></xacro:macro>"));
    const std::filesystem::path document = held.write("top.xacro", include_tree::robot(
        "<xacro:include filename=\"w.xacro\" ns=\"n\"/>" + removes("n") + "<xacro:n.m/>"));
    meios::log_sink silent;
    meios::source_stack sources{};
    const expansion_result out = include_tree::expand_file(document, sources, silent);
    REQUIRE(refuses_with(out, meios::diagnostic_code::xacro_structural_error, "n.m"));
}

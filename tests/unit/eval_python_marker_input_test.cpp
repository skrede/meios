#include "eval_python_fixture.h"

#include "../marker_spelling.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/xacro/container_marker.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <fstream>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

std::string marked(char marker, std::string_view inner)
{
    return std::string(1, marker) + std::string(inner) + marker;
}

// This spelling reaches the binding seam as the very byte the raw spelling does; writing the byte
// here instead would put a control character in the source file.
std::string reference_marked(char one, std::string_view inner)
{
    const std::string escape = marker::as_referenced(one);
    return escape + std::string(inner) + escape;
}

outcome expand_python(std::string_view document, std::optional<std::string> yaml = std::nullopt)
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    return run(document, meios::eval_policy::fail, handle, std::move(yaml));
}

std::string property_document(std::string_view value, std::string_view expression)
{
    return std::string(header) + "<xacro:property name=\"fake\" value=\"" + std::string(value)
        + "\"/><l>" + std::string(expression) + "</l></robot>";
}

void carries_no_marker(const outcome &result)
{
    REQUIRE(result.expanded);
    marker::absent_from(result.expanded->document);
}

}

TEST_CASE("a forged plain marker written as a raw byte leaves a property value a string",
          "[eval_python]")
{
    const std::string value = marked(meios::detail::plain_container_marker, "[1, 2]");
    const outcome resolved = expand_python(property_document(value, "${fake[0]}"));
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>[</l>"));
    REQUIRE_FALSE(leaves(resolved, "<l>1</l>"));
    carries_no_marker(resolved);
}

TEST_CASE("a forged plain marker written as a character reference leaves a property value a string",
          "[eval_python]")
{
    const std::string value = reference_marked(meios::detail::plain_container_marker, "[1, 2]");
    const outcome resolved = expand_python(property_document(value, "${fake[0]}"));
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>[</l>"));
    REQUIRE_FALSE(leaves(resolved, "<l>1</l>"));
    carries_no_marker(resolved);
}

TEST_CASE("a forged provenance marker buys a hand-written mapping no dotted access",
          "[eval_python]")
{
    const std::string value = marked(meios::detail::yaml_container_marker, "{'a': 1}");
    const outcome resolved = expand_python(property_document(value, "${fake.a}"));
    REQUIRE_FALSE(resolved.expanded.has_value());
    REQUIRE(resolved.reported.find("has no attribute 'a'") != std::string::npos);
}

TEST_CASE("the evaluator's own marker still crosses into a binding", "[eval_python]")
{
    const outcome resolved =
        expand_python(property_document("${xacro.load_yaml('limits.yaml')}", "${fake.shoulder.max}"),
                      "shoulder:\n  max: 42\n");
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>42</l>"));
}

TEST_CASE("a forged marker in a declared default is gone before the pre-pass binds it",
          "[eval_python]")
{
    const std::string document = std::string(header) + "<l>${early[0]}</l><xacro:arg name=\"early\" default=\""
        + marked(meios::detail::plain_container_marker, "[1, 2]") + "\"/></robot>";
    const outcome resolved = expand_python(document);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>[</l>"));
    carries_no_marker(resolved);
}

TEST_CASE("a forged marker in a declared default is gone before the declaration resolves it",
          "[eval_python]")
{
    const std::string document = std::string(header) + "<xacro:arg name=\"late\" default=\""
        + marked(meios::detail::plain_container_marker, "[1, ${1+1}]")
        + "\"/><l>${late[0]}</l></robot>";
    const outcome resolved = expand_python(document);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>[</l>"));
    carries_no_marker(resolved);
}

TEST_CASE("a forged marker in a macro argument is gone before the parameter binds", "[eval_python]")
{
    const std::string document = std::string(header)
        + "<xacro:macro name=\"probe\" params=\"v\"><l>${v[0]}</l></xacro:macro>"
        + "<xacro:probe v=\"" + marked(meios::detail::plain_container_marker, "[1, 2]")
        + "\"/></robot>";
    const outcome resolved = expand_python(document);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>[</l>"));
    carries_no_marker(resolved);
}

TEST_CASE("a forged marker in ordinary element text is erased on the way out", "[eval_python]")
{
    const std::string text = marked(meios::detail::plain_container_marker, "[1, 2]");
    const outcome resolved = expand_python(span_document(text));
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>[1, 2]</l>"));
    carries_no_marker(resolved);
}

// A binding is below the boundary, where a marker is by definition the evaluator's own; erasing
// forged text as it crosses is therefore the whole of the defense, not a second line of it.
TEST_CASE("a marker already in a binding is still honored", "[eval_python]")
{
    meios::eval_scope scope;
    scope.set("fake", marked(meios::detail::plain_container_marker, "[1, 2]"));
    const std::optional<std::string> got = evaluate("fake[0]", scope);
    REQUIRE(got);
    REQUIRE(*got == "1");
}

TEST_CASE("a caller-supplied argument carrying a forged marker binds as plain text",
          "[eval_python]")
{
    const std::filesystem::path work =
        std::filesystem::temp_directory_path() / "meios_marker_input";
    std::filesystem::remove_all(work);
    std::filesystem::create_directories(work);
    const std::filesystem::path path = work / "robot.urdf.xacro";
    std::ofstream(path) << "<robot name=\"r\" xmlns:xacro=\"http://ros.org/wiki/xacro\">"
                        << "<link name=\"X${fake[0]}X\"/></robot>";

    meios::load_options opts;
    opts.backend = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    opts.args["fake"] = marked(meios::detail::plain_container_marker, "[1, 2]");
    const meios::expected<meios::load_result, meios::load_error> loaded = meios::load(path, opts);
    std::filesystem::remove_all(work);

    REQUIRE(loaded.has_value());
    REQUIRE(loaded->robot.links.size() == 1);
    REQUIRE(loaded->robot.links.front().name == "X[X");
}

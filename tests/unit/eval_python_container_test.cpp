#include "eval_python_fixture.h"

#include <meios/xacro/container_marker.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("a dict crossing a xacro:property boundary is re-hydrated for nested subscripts",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"config\" value=\"${xacro.load_yaml('limits.yaml')}\"/>"
        + "<xacro:property name=\"section\" value=\"${config['limits']}\"/>"
        + "<l>${section['shoulder']['max']}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle,
                                 "limits:\n  shoulder:\n    max: 42\n");
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>42</l>"));
}

TEST_CASE("a yaml mapping keeps dotted access after a subscript hop across a property boundary",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"config\" value=\"${xacro.load_yaml('limits.yaml')}\"/>"
        + "<xacro:property name=\"section\" value=\"${config['limits']}\"/>"
        + "<l>${section.shoulder.max}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle,
                                 "limits:\n  shoulder:\n    max: 42\n");
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>42</l>"));
}

TEST_CASE("a dotted read of a yaml joint limit resolves after two subscript hops", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"config\" value=\"${xacro.load_yaml('limits.yaml')}\"/>"
        + "<xacro:property name=\"lim\" value=\"${config['limits']['shoulder']}\"/>"
        + "<l>${lim.lower}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle,
                                 "limits:\n  shoulder:\n    lower: -1.5\n");
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>-1.5</l>"));
}

TEST_CASE("a misspelled key under dotted access names the key and the keys that exist",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"config\" value=\"${xacro.load_yaml('limits.yaml')}\"/>"
        + "<xacro:property name=\"section\" value=\"${config['shoulder']}\"/>"
        + "<l>${section.lowre}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle,
                                 "shoulder:\n  lower: -1.5\n  upper: 1.5\n");
    REQUIRE_FALSE(resolved.expanded.has_value());
    REQUIRE(resolved.reported.find("robot.xacro:1:") != std::string::npos);
    REQUIRE(resolved.reported.find("(expression_error)") != std::string::npos);
    REQUIRE(resolved.reported.find("has no key 'lowre'") != std::string::npos);
    REQUIRE(resolved.reported.find("keys are: lower, upper") != std::string::npos);
}

TEST_CASE("a misspelled key under subscript fails exactly as it does under a dot", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"config\" value=\"${xacro.load_yaml('limits.yaml')}\"/>"
        + "<xacro:property name=\"section\" value=\"${config['shoulder']}\"/>"
        + "<l>${section['lowre']}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle,
                                 "shoulder:\n  lower: -1.5\n  upper: 1.5\n");
    REQUIRE_FALSE(resolved.expanded.has_value());
    REQUIRE(resolved.reported.find("has no key 'lowre'") != std::string::npos);
    REQUIRE(resolved.reported.find("keys are: lower, upper") != std::string::npos);
}

TEST_CASE("a mapping wider than the hint bound names six keys and counts the rest",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"config\" value=\"${xacro.load_yaml('limits.yaml')}\"/>"
        + "<l>${config.missing}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle,
                                 "a: 1\nb: 2\nc: 3\nd: 4\ne: 5\nf: 6\ng: 7\nh: 8\n");
    REQUIRE_FALSE(resolved.expanded.has_value());
    REQUIRE(resolved.reported.find("keys are: a, b, c, d, e, f (+2 more)") != std::string::npos);
}

TEST_CASE("an ordinary string binding is never fabricated into a container", "[eval_python]")
{
    meios::eval_scope scope;
    scope.set("v", std::string("6"));
    const std::optional<std::string> got = evaluate("v + '0'", scope);
    REQUIRE(got);
    REQUIRE(*got == "60");
}

TEST_CASE("an authored literal-shaped property value stays a string under subscript",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"x\" value=\"[1, 2]\"/>"
        + "<l>${x[0]}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>[</l>"));
    REQUIRE_FALSE(leaves(resolved, "<l>1</l>"));
}

TEST_CASE("a container emitted whole into element text carries no control-char marker",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome resolved = run(span_document("${[1, 2, 3]}"), meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>[1, 2, 3]</l>"));
    for(char marker : meios::detail::container_markers)
    {
        INFO("marker byte " << static_cast<int>(marker));
        CHECK(resolved.expanded->document.find(marker) == std::string::npos);
    }
}

TEST_CASE("a yaml-sourced mapping emitted whole into element text carries no control-char marker",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"config\" value=\"${xacro.load_yaml('limits.yaml')}\"/>"
        + "<l>${config}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle,
                                 "shoulder:\n  max: 42\n");
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>{'shoulder': {'max': 42}}</l>"));
    for(char marker : meios::detail::container_markers)
    {
        INFO("marker byte " << static_cast<int>(marker));
        CHECK(resolved.expanded->document.find(marker) == std::string::npos);
    }
}

TEST_CASE("a yaml-sourced mapping emitted whole into an attribute value carries no control-char marker",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"config\" value=\"${xacro.load_yaml('limits.yaml')}\"/>"
        + "<l tag=\"${config}\"/></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle,
                                 "shoulder:\n  max: 42\n");
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "tag=\"{'shoulder': {'max': 42}}\""));
    for(char marker : meios::detail::container_markers)
    {
        INFO("marker byte " << static_cast<int>(marker));
        CHECK(resolved.expanded->document.find(marker) == std::string::npos);
    }
}

TEST_CASE("a container emitted whole into an attribute value carries no control-char marker",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header) + "<l tag=\"${[1, 2, 3]}\"/></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "tag=\"[1, 2, 3]\""));
    for(char marker : meios::detail::container_markers)
    {
        INFO("marker byte " << static_cast<int>(marker));
        CHECK(resolved.expanded->document.find(marker) == std::string::npos);
    }
}

TEST_CASE("a list crossing a xacro:property boundary re-hydrates without leaking the marker",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"row\" value=\"${[10, 20, 30]}\"/>"
        + "<l>${row[1]}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>20</l>"));
    for(char marker : meios::detail::container_markers)
    {
        INFO("marker byte " << static_cast<int>(marker));
        CHECK(resolved.expanded->document.find(marker) == std::string::npos);
    }
}
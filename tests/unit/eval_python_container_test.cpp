#include "eval_python_fixture.h"

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
    REQUIRE(resolved.expanded->document.find('\x01') == std::string::npos);
}

TEST_CASE("a container emitted whole into an attribute value carries no control-char marker",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header) + "<l tag=\"${[1, 2, 3]}\"/></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "tag=\"[1, 2, 3]\""));
    REQUIRE(resolved.expanded->document.find('\x01') == std::string::npos);
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
    REQUIRE(resolved.expanded->document.find('\x01') == std::string::npos);
}
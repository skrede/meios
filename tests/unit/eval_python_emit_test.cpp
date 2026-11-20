#include "eval_python_fixture.h"

#include "../marker_spelling.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <latch>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <cstddef>
#include <optional>

TEST_CASE("a container emitted whole into element text carries no control-char marker",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome resolved = run(span_document("${[1, 2, 3]}"), meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>[1, 2, 3]</l>"));
    marker::absent_from(resolved.expanded->document);
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
    marker::absent_from(resolved.expanded->document);
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
    marker::absent_from(resolved.expanded->document);
}

TEST_CASE("a container emitted whole into an attribute value carries no control-char marker",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header) + "<l tag=\"${[1, 2, 3]}\"/></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "tag=\"[1, 2, 3]\""));
    marker::absent_from(resolved.expanded->document);
}

// The subscript alone renders a scalar the marker could never ride out on, so the round-tripped
// list is also emitted whole; its span is matched without the delimiters around it, leaving an
// unstripped marker for the absence check rather than the span to report.
TEST_CASE("a list crossing a xacro:property boundary re-hydrates without leaking the marker",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string document = std::string(header)
        + "<xacro:property name=\"row\" value=\"${[10, 20, 30]}\"/>"
        + "<l tag=\"${row}\">${row[1]}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, ">20</l>"));
    REQUIRE(leaves(resolved, "[10, 20, 30]"));
    marker::absent_from(resolved.expanded->document);
}

TEST_CASE("eight threads making the process's first evaluations at once each render the sum", "[eval_python]")
{
    std::array<std::optional<std::string>, 8> answers;
    std::latch running(static_cast<std::ptrdiff_t>(answers.size()));
    {
        std::vector<std::jthread> threads;
        for(std::optional<std::string> &answer : answers)
            threads.emplace_back([&running, &answer] {
                running.arrive_and_wait();
                answer = evaluate("1 + 1", meios::eval_scope{});
            });
    }
    for(const std::optional<std::string> &answer : answers)
        CHECK(answer == std::optional<std::string>("2"));
}

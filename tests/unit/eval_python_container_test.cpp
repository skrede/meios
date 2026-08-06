#include "eval_python_fixture.h"

#include <meios/xacro/container_marker.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <functional>
#include <string_view>

namespace
{

std::string yaml_document(std::string_view expression)
{
    return std::string(header)
        + "<xacro:property name=\"config\" value=\"${xacro.load_yaml('limits.yaml')}\"/>"
        + "<l>" + std::string(expression) + "</l></robot>";
}

outcome expand_span(std::string_view expression)
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    return run(span_document(expression), meios::eval_policy::fail, handle);
}

outcome expand_over_yaml(std::string_view expression, std::string yaml)
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    return run(yaml_document(expression), meios::eval_policy::fail, handle, std::move(yaml));
}

// The shared fixture's tally counts errors only, so a case about a warning needs a sink that
// keeps every level and whether the record arrived with a position.
struct journal
{
    struct record
    {
        meios::level lvl;
        std::string where;
        std::string message;
    };

    std::vector<record> records;

    void operator()(meios::level lvl, const std::string &message)
    {
        records.push_back({ lvl, std::string{}, message });
    }

    void operator()(meios::level lvl, const meios::source_location &at, const std::string &message)
    {
        records.push_back({ lvl, meios::to_string(at), message });
    }
};

struct journaled
{
    expansion_result expanded;
    std::vector<journal::record> records;
};

journaled expand_journalled(std::string_view document, meios::eval_policy policy,
                            std::optional<std::string> yaml = std::nullopt)
{
    journal recorded;
    meios::log_sink_f sink{ std::ref(recorded) };
    meios::eval_scope scope;
    if(yaml)
        install_yaml(scope, std::move(*yaml));
    meios::source_stack sources;
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    expansion_result out = meios::expand(document, scope, sources, "robot.xacro",
                                         meios::expansion_limits{}, policy, handle, sink);
    return journaled{ std::move(out), std::move(recorded.records) };
}

std::vector<journal::record> warnings(const journaled &result)
{
    std::vector<journal::record> picked;
    for(const journal::record &one : result.records)
    {
        if(one.lvl == meios::level::warn)
            picked.push_back(one);
    }
    return picked;
}

}

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

TEST_CASE("a set is refused rather than emitted as a container it is not", "[eval_python]")
{
    const outcome resolved = expand_span("${set([1, 2])}");
    REQUIRE_FALSE(resolved.expanded.has_value());
    REQUIRE(resolved.reported.find("failed to convert its result: a set is not a serializable expression result")
            != std::string::npos);
    REQUIRE(resolved.reported.find("{1, 2}") == std::string::npos);
}

TEST_CASE("no address-carrying result reaches a flattened document", "[eval_python]")
{
    const outcome zipped = expand_span("${zip([1], [2])}");
    REQUIRE_FALSE(zipped.expanded.has_value());
    CHECK(zipped.reported.find("a zip is not a serializable expression result") != std::string::npos);

    const outcome counted = expand_span("${enumerate([1, 2])}");
    REQUIRE_FALSE(counted.expanded.has_value());
    CHECK(counted.reported.find("a enumerate is not a serializable expression result") != std::string::npos);

    const outcome method = expand_over_yaml("${config.items}", "shoulder:\n  max: 42\n");
    REQUIRE_FALSE(method.expanded.has_value());
    CHECK(method.reported.find("a builtin_function_or_method is not a serializable expression result")
          != std::string::npos);

    for(const outcome *one : { &zipped, &counted, &method })
        CHECK(one->reported.find("0x") == std::string::npos);
}

TEST_CASE("a dict view is refused without dumping the configuration it would have rendered",
          "[eval_python]")
{
    const outcome resolved = expand_over_yaml("${config.values()}", "limits:\n  shoulder:\n    max: 42\n");
    REQUIRE_FALSE(resolved.expanded.has_value());
    REQUIRE(resolved.reported.find("a dict_values is not a serializable expression result")
            != std::string::npos);
    REQUIRE(resolved.reported.find("'max'") == std::string::npos);
    REQUIRE(resolved.reported.find("42") == std::string::npos);
}

TEST_CASE("a range object and a bare mathematics function are both refused", "[eval_python]")
{
    const outcome ranged = expand_span("${range(3)}");
    REQUIRE_FALSE(ranged.expanded.has_value());
    CHECK(ranged.reported.find("a range is not a serializable expression result") != std::string::npos);

    const outcome bare = expand_span("${sqrt}");
    REQUIRE_FALSE(bare.expanded.has_value());
    CHECK(bare.reported.find("a builtin_function_or_method is not a serializable expression result")
          != std::string::npos);
}

TEST_CASE("every permitted result type renders exactly as it did before the set was closed",
          "[eval_python]")
{
    const std::pair<std::string_view, std::string_view> rows[] = {
        { "${2**64}", "<l>18446744073709551616</l>" },
        { "${1.5}", "<l>1.5</l>" },
        { "${True}", "<l>True</l>" },
        { "${'text'}", "<l>text</l>" },
        { "${[1, 2]}", "<l>[1, 2]</l>" },
        { "${ {'a': 1} }", "<l>{'a': 1}</l>" },
        { "${(1, 2)}", "<l>(1, 2)</l>" },
    };
    for(const std::pair<std::string_view, std::string_view> &row : rows)
    {
        INFO(row.first);
        const outcome resolved = expand_span(row.first);
        REQUIRE(resolved.expanded.has_value());
        CHECK(leaves(resolved, row.second));
    }
}

TEST_CASE("a null result serializes as None and says so at a position", "[eval_python]")
{
    const journaled resolved = expand_journalled(span_document("${None}"), meios::eval_policy::fail);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(resolved.expanded->document.find("<l>None</l>") != std::string::npos);
    const std::vector<journal::record> warned = warnings(resolved);
    REQUIRE(warned.size() == 1);
    REQUIRE(warned.front().message.find("the expression result is null") != std::string::npos);
    REQUIRE(warned.front().where.find("robot.xacro:1:") != std::string::npos);
}

TEST_CASE("a yaml key written with no value flattens as None under every policy", "[eval_python]")
{
    const meios::eval_policy policies[] = { meios::eval_policy::fail, meios::eval_policy::warn,
                                            meios::eval_policy::skip };
    for(meios::eval_policy policy : policies)
    {
        const journaled resolved = expand_journalled(yaml_document("${config['empty']}"), policy,
                                                     "empty:\n");
        REQUIRE(resolved.expanded.has_value());
        CHECK(resolved.expanded->document.find("<l>None</l>") != std::string::npos);
        const std::vector<journal::record> warned = warnings(resolved);
        REQUIRE(warned.size() == 1);
        CHECK(warned.front().message.find("the expression result is null") != std::string::npos);
        CHECK(warned.front().where.find("robot.xacro:1:") != std::string::npos);
    }
}

TEST_CASE("a string spelled None is a string and reports nothing", "[eval_python]")
{
    const journaled resolved = expand_journalled(span_document("${'None'}"), meios::eval_policy::fail);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(resolved.expanded->document.find("<l>None</l>") != std::string::npos);
    REQUIRE(resolved.records.empty());
}
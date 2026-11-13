#include "eval_python_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <utility>
#include <string_view>

namespace
{

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
    CHECK(counted.reported.find("an enumerate is not a serializable expression result") != std::string::npos);

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

TEST_CASE("a property bound to a wide integer result renders its digits when read back", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const std::string source =
        std::string(header) + R"(<xacro:property name="c" value="${2**70}"/><l>c_${c}</l></robot>)";
    const outcome resolved = run(source, meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    CHECK(leaves(resolved, "<l>c_1180591620717411303424</l>"));
}

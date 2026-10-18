#include "error_sink.h"
#include "eval_python_either_fixture.h"

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include <memory>
#include <string>
#include <optional>
#include <string_view>

using namespace either_evaluator;

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

const meios::source_location hosting{"robot.xacro", 1, 0};

meios::text_resource_result crossed()
{
    return meios::unexpected<meios::text_resource_failure>(meios::text_resource_failure::exhausted);
}

template<typename E>
expansion_result expand_under(meios::eval_policy policy, meios::eval_scope &scope, meios::log_sink &heard)
{
    const auto handle = std::make_shared<meios::evaluator_handle>(E{});
    meios::source_stack sources;
    const std::string document = "<robot xmlns:xacro=\"http://ros.org/wiki/xacro\"><l>${load_yaml('big.yaml')}</l></robot>";
    return meios::expand(document, scope, sources, "robot.xacro", meios::expansion_limits{}, policy, handle, heard);
}

}

TEMPLATE_TEST_CASE("a yaml spec escaping containment is refused by either evaluator", "[eval_python]", meios::python_evaluator, meios::unrestricted_python_evaluator)
{
    const meios::eval_scope scope = serving(meios::unexpected<meios::text_resource_failure>(meios::text_resource_failure::uncontained));
    const verdict got             = refuse_of<TestType>("load_yaml('../../../etc/passwd')", scope);
    REQUIRE(got.refused);
    REQUIRE(names(got, "uncontained-yaml-path"));
    REQUIRE(names(got, "../../../etc/passwd"));
}

TEMPLATE_TEST_CASE("a scope carrying no loader ends a yaml request on one located unresolved record under either evaluator", "[eval_python]", meios::python_evaluator,
                   meios::unrestricted_python_evaluator)
{
    for(meios::eval_policy policy : {meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip})
    {
        INFO("policy " << static_cast<int>(policy));
        meios::eval_scope bare;
        error_sink_test::error_sink heard;
        const expansion_result out = expand_under<TestType>(policy, bare, heard);
        error_sink_test::ends_on_one_record(out, heard, meios::diagnostic_code::unresolved_asset, hosting);
        for(const error_sink_test::error_record &one : heard.errors)
        {
            CHECK(one.message.find("big.yaml") != std::string::npos);
            CHECK(one.message.find("uncontained-yaml-path") == std::string::npos);
        }
    }
}

TEMPLATE_TEST_CASE("an unreachable document comes back from either evaluator as an error on its own record", "[eval_python]", meios::python_evaluator,
                   meios::unrestricted_python_evaluator)
{
    const meios::eval_scope scope = serving(meios::unexpected<meios::text_resource_failure>(meios::text_resource_failure::unreachable));
    error_sink_test::error_sink heard;
    TestType evaluator;
    const meios::text_outcome got = evaluator.eval_to_text("load_yaml('absent.yaml')", scope, heard);
    CHECK_FALSE(got.text.has_value());
    CHECK(got.failure == meios::eval_failure_kind::error);
    REQUIRE(heard.errors.size() == 1);
    CHECK(heard.errors.front().code == meios::diagnostic_code::unresolved_asset);
    CHECK(heard.errors.front().message.find("absent.yaml") != std::string::npos);
}

TEMPLATE_TEST_CASE("a resolvable yaml spec loads its unit tags identically under either evaluator", "[eval_python]", meios::python_evaluator, meios::unrestricted_python_evaluator)
{
    const meios::eval_scope scope = serving("a: !radians 1.5\n"
                                            "b: !degrees 180\n"
                                            "c: !meters 2.0\n"
                                            "d: !millimeters 1000\n"
                                            "e: !foot 1\n"
                                            "f: !inches 1\n");
    const char *tagged[]          = {
            "abs(load_yaml('u')['a'] - 1.5) < 1e-9", "abs(load_yaml('u')['b'] - pi) < 1e-9",     "abs(load_yaml('u')['c'] - 2.0) < 1e-9",
            "abs(load_yaml('u')['d'] - 1.0) < 1e-9", "abs(load_yaml('u')['e'] - 0.3048) < 1e-9", "abs(load_yaml('u')['f'] - 0.0254) < 1e-9",
    };
    for(const char *expr : tagged)
    {
        INFO(expr);
        const std::optional<std::string> got = evaluate<TestType>(expr, scope);
        REQUIRE(got);
        REQUIRE(*got == "True");
    }
}

TEMPLATE_TEST_CASE("a read crossing the loader's ceiling ends the load on one located budget record under either evaluator", "[eval_python]", meios::python_evaluator,
                   meios::unrestricted_python_evaluator)
{
    for(meios::eval_policy policy : {meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip})
    {
        INFO("policy " << static_cast<int>(policy));
        meios::eval_scope scope = serving(crossed());
        error_sink_test::error_sink heard;
        const expansion_result out = expand_under<TestType>(policy, scope, heard);
        error_sink_test::ends_on_one_record(out, heard, meios::diagnostic_code::expansion_budget_exceeded, hosting);
    }
}

TEMPLATE_TEST_CASE("a read crossing the loader's ceiling comes back from either evaluator as exhausted", "[eval_python]", meios::python_evaluator, meios::unrestricted_python_evaluator)
{
    const meios::eval_scope scope = serving(crossed());
    error_sink_test::error_sink heard;
    TestType evaluator;
    const meios::text_outcome got = evaluator.eval_to_text("load_yaml('big.yaml')", scope, heard);
    CHECK_FALSE(got.text.has_value());
    CHECK(got.failure == meios::eval_failure_kind::exhausted);
    CHECK(heard.errors.size() == 1);
}

TEST_CASE("an unrestricted expression that swallows the raise still ends on the crossed ceiling", "[eval_python]")
{
    const meios::eval_scope scope = serving(crossed());
    error_sink_test::error_sink heard;
    meios::unrestricted_python_evaluator evaluator;
    const meios::text_outcome got = evaluator.eval_to_text("exec(\"try:\\n    load_yaml('big.yaml')\\nexcept Exception:\\n    pass\") or 'swallowed'", scope, heard);
    CHECK_FALSE(got.text.has_value());
    CHECK(got.failure == meios::eval_failure_kind::exhausted);
    CHECK(heard.errors.size() == 1);
}

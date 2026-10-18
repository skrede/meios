#include "native_expression_fixture.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <functional>
#include <string_view>

using native_expression_test::evaluate;
using native_expression_test::outcome;
using native_expression_test::recorder;
using native_expression_test::seeded_scope;

namespace
{

// The origin mark is not observable through a rendering, so this reaches the value itself: the
// clause under test is that a constructed mapping carries none, at any depth.
meios::value evaluated(std::string_view expression)
{
    recorder heard;
    meios::log_sink_f sink{std::ref(heard)};
    const meios::eval_scope scope = seeded_scope(sink);
    meios::core_evaluator evaluator;
    return evaluator.eval(expression, scope, sink);
}

}

TEST_CASE("a keyword-argument constructor builds a mapping the evaluator can subscript", "[native][expression]")
{
    CHECK(evaluate("dict(a=1, b=2)").rendered == "{'a': 1, 'b': 2}");
    CHECK(evaluate("dict(a=1, b=2)['a']").rendered == "1");
    CHECK(evaluate("dict(a=1, b=2)['b']").rendered == "2");
    CHECK(evaluate("dict()").rendered == "{}");
    CHECK(evaluate("dict(a=dict(b=2)['b'])").rendered == "{'a': 2}");
    CHECK(evaluate("dict(mesh=sec_mesh_files['base'])['mesh']['visual']['mesh']['package']").rendered == "ur_description");
    CHECK(evaluate("'a' in dict(a=1)").rendered == "True");
}

TEST_CASE("the mapping a constructor builds carries no document origin and has no member path", "[native][expression]")
{
    const meios::value built = evaluated("dict(a=1, b=dict(c=2))");

    REQUIRE(built.kind() == meios::value_kind::mapping);
    CHECK_FALSE(built.from_yaml());
    REQUIRE(built.at(std::string_view{"b"}).has_value());
    CHECK_FALSE(built.at(std::string_view{"b"})->from_yaml());

    const outcome dotted = evaluate("dict(a=1).a");
    CHECK(dotted.failed);
    CHECK(dotted.kind == meios::eval_failure_kind::unsupported);
    REQUIRE_FALSE(dotted.messages.empty());
    CHECK(dotted.messages.front().find("not an auxiliary document") != std::string::npos);
    CHECK(evaluate("dict(a=1)['a']").rendered == "1");
}

TEST_CASE("a repeated keyword name refuses rather than resolving to one of its values", "[native][expression]")
{
    const outcome repeated = evaluate("dict(a=1, a=2)");

    CHECK(repeated.failed);
    CHECK(repeated.kind == meios::eval_failure_kind::error);
    REQUIRE_FALSE(repeated.messages.empty());
    CHECK(repeated.messages.front().find("keyword argument is repeated") != std::string::npos);
    CHECK(evaluate("dict(a=1, b=1)").rendered == "{'a': 1, 'b': 1}");
}

TEST_CASE("an empty constructor call builds an empty mapping and subscripting it refuses", "[native][expression]")
{
    const outcome empty = evaluate("dict()['missing']");

    CHECK(evaluate("dict()").rendered == "{}");
    CHECK(empty.failed);
    REQUIRE(empty.codes.size() == 1);
    CHECK(empty.codes.front() == meios::diagnostic_code::undefined_property);
    REQUIRE_FALSE(empty.messages.empty());
    CHECK(empty.messages.front().find("'missing'") != std::string::npos);
}

TEST_CASE("every constructor argument shape but the keyword one refuses naming what it met", "[native][expression]")
{
    const std::pair<std::string_view, std::string_view> refused[] = {{"dict({'a': 1})", "not '{'"}, {"dict([('a', 1)])", "not '['"}, {"dict([('a', 1)], b=2)", "not '['"},
                                                                     {"dict(a=1, 2)", "not '2'"},   {"dict(1=2)", "not '1'"},        {"dict(a=1, )", "not ')'"}};

    for(const std::pair<std::string_view, std::string_view> &one : refused)
    {
        INFO("expression: " << one.first);
        const outcome ran = evaluate(one.first);
        CHECK(ran.failed);
        CHECK(ran.kind == meios::eval_failure_kind::unsupported);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find("keyword arguments only") != std::string::npos);
        CHECK(ran.messages.front().find(one.second) != std::string::npos);
    }
}

TEST_CASE("the constructor spellings with no representable result still refuse by name", "[native][expression]")
{
    for(std::string_view form : {"list()", "list([1])", "set()", "tuple(1, 2)"})
    {
        INFO("expression: " << form);
        const outcome ran = evaluate(form);
        CHECK(ran.failed);
        CHECK(ran.kind == meios::eval_failure_kind::unsupported);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find("the Python constructor '") != std::string::npos);
    }
}

TEST_CASE("a constructor call missing its closing parenthesis or its value refuses as a fault", "[native][expression]")
{
    const outcome unclosed  = evaluate("dict(a=1");
    const outcome valueless = evaluate("dict(a=)");

    CHECK(unclosed.failed);
    REQUIRE_FALSE(unclosed.messages.empty());
    CHECK(unclosed.messages.front().find("expected ')'") != std::string::npos);
    CHECK(valueless.failed);
    REQUIRE_FALSE(valueless.messages.empty());
    CHECK(valueless.messages.front().find("expression") != std::string::npos);
}

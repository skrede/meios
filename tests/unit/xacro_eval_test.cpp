#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <clocale>
#include <optional>
#include <string_view>

namespace
{

struct mock_evaluator
{
    meios::value eval(std::string_view, const meios::eval_scope &, meios::log_sink &) const
    {
        return meios::value{ 0ll };
    }
};

struct not_an_evaluator
{
    int eval(int) const { return 0; }
};

}

static_assert(meios::expression_evaluator<mock_evaluator>);
static_assert(!meios::expression_evaluator<not_an_evaluator>);
static_assert(!meios::locates_errors<mock_evaluator>);

TEST_CASE("the numeric shim parses and prints floats locale-independently", "[xacro][numeric]")
{
    bool ok = false;

    REQUIRE(meios::detail::parse_double("0.5", ok) == 0.5);
    REQUIRE(ok);

    meios::detail::parse_double("x", ok);
    REQUIRE_FALSE(ok);

    REQUIRE(meios::detail::print_double(0.5) == "0.5");
    REQUIRE(meios::detail::print_double(1.0) == "1.0");
    REQUIRE(meios::detail::print_double(2.0) == "2.0");

    const std::string round_trip = meios::detail::print_double(0.1);
    bool back_ok = false;
    REQUIRE(meios::detail::parse_double(round_trip, back_ok) == 0.1);
    REQUIRE(back_ok);
}

TEST_CASE("parse_int rejects non-integer literals", "[xacro][numeric]")
{
    bool ok = false;

    REQUIRE(meios::detail::parse_int("42", ok) == 42);
    REQUIRE(ok);

    meios::detail::parse_int("4.0", ok);
    REQUIRE_FALSE(ok);
}

TEST_CASE("numbers survive a comma-decimal global locale", "[xacro][numeric]")
{
    const char *installed = std::setlocale(LC_ALL, "de_DE.UTF-8");
    if(installed == nullptr)
        installed = std::setlocale(LC_ALL, "de_DE");
    if(installed == nullptr)
    {
        SUCCEED("no comma-decimal locale available on this host");
        return;
    }

    bool ok = false;
    REQUIRE(meios::detail::parse_double("0.5", ok) == 0.5);
    REQUIRE(ok);
    REQUIRE(meios::detail::print_double(0.5) == "0.5");
    REQUIRE(meios::detail::print_double(1.0) == "1.0");

    std::setlocale(LC_ALL, "C");
}

TEST_CASE("value prints int/float/bool as Python str() does", "[xacro][value]")
{
    REQUIRE(meios::to_python_str(meios::value{ 2ll }) == "2");
    REQUIRE(meios::to_python_str(meios::value{ 2.0 }) == "2.0");
    REQUIRE(meios::to_python_str(meios::value{ 0.5 }) == "0.5");
    REQUIRE(meios::to_python_str(meios::value{ true }) == "True");
    REQUIRE(meios::to_python_str(meios::value{ false }) == "False");
}

TEST_CASE("eval_scope resolves a set name and reports an unset one absent", "[xacro][value]")
{
    meios::eval_scope scope;
    scope.set("radius", meios::value{ 0.2 });
    scope.set("prefix", std::string{ "arm" });

    REQUIRE(scope.contains("radius"));
    REQUIRE_FALSE(scope.contains("length"));

    const std::optional<meios::binding> radius = scope.lookup("radius");
    REQUIRE(radius.has_value());
    REQUIRE(std::get<meios::value>(*radius) == meios::value{ 0.2 });

    const std::optional<meios::binding> prefix = scope.lookup("prefix");
    REQUIRE(prefix.has_value());
    REQUIRE(std::get<std::string>(*prefix) == "arm");

    REQUIRE_FALSE(scope.lookup("length").has_value());
}

TEST_CASE("a type exposing eval(expr, scope, log) satisfies expression_evaluator", "[xacro][concept]")
{
    mock_evaluator evaluator;
    meios::eval_scope scope;
    meios::log_sink log;

    REQUIRE(std::holds_alternative<long long>(evaluator.eval("0", scope, log)));
    STATIC_REQUIRE(meios::expression_evaluator<mock_evaluator>);
    STATIC_REQUIRE_FALSE(meios::expression_evaluator<not_an_evaluator>);
}

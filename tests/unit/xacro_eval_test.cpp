#include <meios/xacro.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <clocale>
#include <optional>
#include <functional>
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

std::string eval_str(std::string_view expression, const meios::eval_scope &scope = {})
{
    meios::core_evaluator evaluator;
    meios::log_sink sink;
    return meios::to_python_str(evaluator.eval(expression, scope, sink));
}

struct failure
{
    bool failed{ false };
    int diagnostics{ 0 };
    std::string message{};

    void operator()(meios::level, const std::string &text)
    {
        message = text;
        ++diagnostics;
    }

    void operator()(meios::level, const meios::source_location &, const std::string &text)
    {
        message = text;
        ++diagnostics;
    }

    void operator()(meios::level, meios::diagnostic_code, const meios::source_location &,
                    const std::string &text)
    {
        message = text;
        ++diagnostics;
    }
};

failure eval_failure(std::string_view expression)
{
    failure state;
    meios::core_evaluator evaluator;
    meios::eval_scope scope;
    meios::log_sink_f sink{ std::ref(state) };
    evaluator.eval(expression, scope, sink);
    state.failed = evaluator.failed();
    return state;
}

meios::eval_failure_kind eval_kind(std::string_view expression)
{
    meios::core_evaluator evaluator;
    meios::eval_scope scope;
    meios::log_sink sink;
    evaluator.eval(expression, scope, sink);
    return evaluator.failure_kind();
}

}

static_assert(meios::expression_evaluator<meios::core_evaluator>);

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
    STATIC_REQUIRE(meios::expression_evaluator<meios::core_evaluator>);
}

TEST_CASE("core evaluator reproduces CPython numeric semantics", "[xacro][eval]")
{
    REQUIRE(eval_str("1/2") == "0.5");
    REQUIRE(eval_str("4/2") == "2.0");
    REQUIRE(eval_str("2+3*4") == "14");
    REQUIRE(eval_str("-7//2") == "-4");
    REQUIRE(eval_str("-7%3") == "2");
    REQUIRE(eval_str("-2**2") == "-4");
    REQUIRE(eval_str("2**-2") == "0.25");
    REQUIRE(eval_str("1<2<3") == "True");
    REQUIRE(eval_str("3<2<1") == "False");
    REQUIRE(eval_str("1 if 0 else 2") == "2");
    REQUIRE(eval_str("True and 1<2") == "True");
    REQUIRE(eval_str("not 0") == "True");
    REQUIRE(eval_str("(2+3)*4") == "20");
}

TEST_CASE("integer power bounds its work and loud-fails on an unrepresentable result", "[xacro][eval]")
{
    REQUIRE(eval_str("2**10") == "1024");
    REQUIRE(eval_str("10**18") == "1000000000000000000");
    REQUIRE(eval_str("0**5") == "0");
    REQUIRE(eval_str("1**1000000000") == "1");
    REQUIRE(eval_str("(-1)**1000000001") == "-1");

    const failure overflow = eval_failure("9**99999999999");
    REQUIRE(overflow.failed);
    REQUIRE(overflow.diagnostics >= 1);
    REQUIRE(eval_kind("9**99999999999") == meios::eval_failure_kind::error);
}

TEST_CASE("core evaluator dispatches the whitelisted math functions", "[xacro][eval]")
{
    REQUIRE(eval_str("pi") == "3.141592653589793");
    REQUIRE(eval_str("sqrt(4)") == "2.0");
    REQUIRE(eval_str("radians(180)") == "3.141592653589793");
    REQUIRE(eval_str("degrees(pi)") == "180.0");
    REQUIRE(eval_str("floor(2.7)") == "2");
    REQUIRE(eval_str("ceil(2.1)") == "3");
    REQUIRE(eval_str("abs(-3)") == "3");
    REQUIRE(eval_str("min(1,2)") == "1");
    REQUIRE(eval_str("max(1.5,2)") == "2");
    REQUIRE(eval_str("atan2(0,1)") == "0.0");
}

TEST_CASE("core evaluator follows CPython result typing", "[xacro][typing]")
{
    REQUIRE(eval_str("2+2") == "4");
    REQUIRE(eval_str("4/2") == "2.0");
    REQUIRE(eval_str("2**3") == "8");
    REQUIRE(eval_str("2**-1") == "0.5");
    REQUIRE(eval_str("True+1") == "2");
    REQUIRE(eval_str("7//2") == "3");
    REQUIRE(eval_str("floor(3.0)") == "3");
    REQUIRE(eval_str("1<2") == "True");

    meios::eval_scope scope;
    scope.set("radius", meios::value{ 0.2 });
    REQUIRE(eval_str("radius*2", scope) == "0.4");
}

TEST_CASE("multi-character operators lex as single tokens through the evaluator", "[xacro][lexer]")
{
    REQUIRE(eval_str("2**3**2") == "512");
    REQUIRE(eval_str("7//2") == "3");
    REQUIRE(eval_str("1<=2<3") == "True");
    REQUIRE(eval_str("2!=3") == "True");
    REQUIRE(eval_str("3>=3") == "True");
}

TEST_CASE("an unsupported construct loud-fails instead of guessing", "[xacro][eval]")
{
    const failure unknown_call = eval_failure("foo(1)");
    REQUIRE(unknown_call.failed);
    REQUIRE(unknown_call.diagnostics >= 1);

    const failure list_literal = eval_failure("[1, 2]");
    REQUIRE(list_literal.failed);

    const failure absent_name = eval_failure("undefined_property");
    REQUIRE(absent_name.failed);
    REQUIRE(absent_name.diagnostics >= 1);
}

TEST_CASE("the lexer separates an unsupported lexeme from a genuine error", "[xacro][lexer]")
{
    REQUIRE(eval_kind("['x']") == meios::eval_failure_kind::unsupported);
    REQUIRE(eval_kind("{'a': 1}") == meios::eval_failure_kind::unsupported);
    REQUIRE(eval_kind("1.2.3") == meios::eval_failure_kind::error);
    REQUIRE(eval_kind("undefined_property") == meios::eval_failure_kind::error);
}

#include <meios/xacro.h>

#include <rapidcheck.h>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>

namespace
{

meios::value eval(const std::string &expression)
{
    meios::core_evaluator evaluator;
    meios::eval_scope scope;
    meios::log_sink sink;
    meios::value result = evaluator.eval(expression, scope, sink);
    RC_ASSERT_FALSE(evaluator.failed());
    return result;
}

std::string paren(long long number)
{
    return "(" + std::to_string(number) + ")";
}

}

TEST_CASE("core evaluator holds algebraic invariants inside the supported subset", "[xacro][property]")
{
    REQUIRE(rc::check("integer addition commutes", [] {
        const long long a = *rc::gen::inRange<long long>(-1000000, 1000000);
        const long long b = *rc::gen::inRange<long long>(-1000000, 1000000);
        RC_ASSERT(meios::to_python_str(eval(paren(a) + "+" + paren(b)))
               == meios::to_python_str(eval(paren(b) + "+" + paren(a))));
    }));

    REQUIRE(rc::check("integer addition and multiplication keep their identities", [] {
        const long long a = *rc::gen::inRange<long long>(-1000000, 1000000);
        RC_ASSERT(meios::to_python_str(eval(paren(a) + "+0")) == std::to_string(a));
        RC_ASSERT(meios::to_python_str(eval(paren(a) + "*1")) == std::to_string(a));
    }));

    REQUIRE(rc::check("evaluation is deterministic", [] {
        const long long a = *rc::gen::inRange<long long>(-1000000, 1000000);
        const long long b = *rc::gen::inRange<long long>(-100, 100);
        const std::string expression = paren(a) + "*" + paren(b) + "-" + paren(a);
        RC_ASSERT(meios::to_python_str(eval(expression)) == meios::to_python_str(eval(expression)));
    }));

    REQUIRE(rc::check("a printed float parses back to itself through the evaluator", [] {
        const double d = static_cast<double>(*rc::gen::inRange<long long>(-100000000, 100000000)) / 1000.0;
        const meios::value back = eval(meios::detail::print_double(d));
        RC_ASSERT(std::holds_alternative<double>(back));
        RC_ASSERT(std::get<double>(back) == d);
    }));
}

#include <meios/xacro.h>

#include <rapidcheck.h>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstdint>
#include <optional>

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

std::string render(const meios::value &result)
{
    return meios::render_scalar(result).value_or(std::string{});
}

std::string paren(std::int64_t number)
{
    return "(" + std::to_string(number) + ")";
}

}

TEST_CASE("core evaluator holds algebraic invariants inside the supported subset", "[xacro][property]")
{
    REQUIRE(rc::check("integer addition commutes", [] {
        const std::int64_t a = *rc::gen::inRange<std::int64_t>(-1000000, 1000000);
        const std::int64_t b = *rc::gen::inRange<std::int64_t>(-1000000, 1000000);
        RC_ASSERT(render(eval(paren(a) + "+" + paren(b)))
               == render(eval(paren(b) + "+" + paren(a))));
    }));

    REQUIRE(rc::check("integer addition and multiplication keep their identities", [] {
        const std::int64_t a = *rc::gen::inRange<std::int64_t>(-1000000, 1000000);
        RC_ASSERT(render(eval(paren(a) + "+0")) == std::to_string(a));
        RC_ASSERT(render(eval(paren(a) + "*1")) == std::to_string(a));
    }));

    REQUIRE(rc::check("evaluation is deterministic", [] {
        const std::int64_t a = *rc::gen::inRange<std::int64_t>(-1000000, 1000000);
        const std::int64_t b = *rc::gen::inRange<std::int64_t>(-100, 100);
        const std::string expression = paren(a) + "*" + paren(b) + "-" + paren(a);
        RC_ASSERT(render(eval(expression)) == render(eval(expression)));
    }));

    REQUIRE(rc::check("a printed float parses back to itself through the evaluator", [] {
        const double d = static_cast<double>(*rc::gen::inRange<std::int64_t>(-100000000, 100000000)) / 1000.0;
        const meios::value back = eval(meios::detail::print_double(d));
        RC_ASSERT(back.kind() == meios::value_kind::real);
        RC_ASSERT(back.real() == d);
    }));
}

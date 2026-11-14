#include <meios/xacro.h>

#include <rapidcheck.h>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace
{

// The limbs at the edges of a 32-bit digit reach the rare corrections of a long division, so they
// are drawn as often as every other limb together.
rc::Gen<std::uint32_t> limb()
{
    return rc::gen::oneOf(rc::gen::element<std::uint32_t>(0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu), rc::gen::arbitrary<std::uint32_t>());
}

std::string operand()
{
    const auto count = *rc::gen::inRange<std::size_t>(1, 7);
    const std::vector<std::uint32_t> limbs = *rc::gen::container<std::vector<std::uint32_t>>(count, limb());
    std::string expression = std::to_string(limbs.back());
    for(std::size_t i = limbs.size() - 1; i-- > 0;)
        expression = "(" + expression + ") * 4294967296 + " + std::to_string(limbs[i]);
    return *rc::gen::arbitrary<bool>() ? "(-(" + expression + "))" : "(" + expression + ")";
}

bool holds(const std::string &expression)
{
    meios::core_evaluator evaluator;
    const meios::eval_scope scope;
    meios::log_sink sink;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return !evaluator.failed() && meios::render_scalar(result) == std::optional<std::string>("True");
}

}

TEST_CASE("floor division and remainder of integers of any size keep the division identity", "[xacro][property]")
{
    REQUIRE(rc::check("the remainder completes the quotient, is smaller than the divisor and takes its sign", [] {
        const std::string a = operand(), b = operand();
        RC_PRE(!holds(b + " == 0"));
        const std::string remainder = "(" + a + " % " + b + ")";
        RC_ASSERT(holds("(" + a + " // " + b + ") * " + b + " + " + remainder + " == " + a));
        RC_ASSERT(holds("abs(" + remainder + ") < abs(" + b + ")"));
        RC_ASSERT(holds(remainder + " == 0 or (" + remainder + " < 0) == (" + b + " < 0)"));
    }));
}

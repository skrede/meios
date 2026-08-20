#include "arith_probe.h"

#include <meios/xacro/detail/numeric.h>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>

namespace
{
constexpr const char *most_negative = "(-9223372036854775807 - 1)";
constexpr const char *outside_range = "outside the range";
}

TEST_CASE("a product past the representable range refuses instead of wrapping", "[native][arith]")
{
    const arith::outcome ran = arith::evaluate("9223372036854775807 * 2");

    CHECK(ran.failed);
    REQUIRE(ran.messages.size() == 1);
    CHECK(ran.messages.front().find(outside_range) != std::string::npos);
    CHECK(arith::evaluate("4611686018427387904 * -4").failed);
}

TEST_CASE("the most negative integer over minus one refuses instead of aborting", "[native][arith]")
{
    const arith::outcome divided = arith::evaluate(std::string(most_negative) + " // -1");
    const arith::outcome remainder = arith::evaluate(std::string(most_negative) + " % -1");

    CHECK(divided.failed);
    REQUIRE(divided.messages.size() == 1);
    CHECK(divided.messages.front().find(outside_range) != std::string::npos);
    CHECK(remainder.failed);
    CHECK(remainder.messages.size() == 1);
}

TEST_CASE("negating and taking the size of the most negative integer refuse", "[native][arith]")
{
    const arith::outcome negated = arith::evaluate(std::string("-") + most_negative);
    const arith::outcome sized = arith::evaluate(std::string("abs(") + most_negative + ")");

    CHECK(negated.failed);
    CHECK(negated.messages.size() == 1);
    CHECK(sized.failed);
    REQUIRE(sized.messages.size() == 1);
    CHECK(sized.messages.front().find(outside_range) != std::string::npos);
}

TEST_CASE("adding and subtracting past either end refuse", "[native][arith]")
{
    CHECK(arith::evaluate("9223372036854775807 + 1").failed);
    CHECK(arith::evaluate(std::string(most_negative) + " - 1").failed);
    CHECK(arith::evaluate(std::string(most_negative) + " + -1").failed);
    CHECK(arith::evaluate("9223372036854775807 - -1").failed);
}

TEST_CASE("exponentiation refuses past the range with the message it always used",
          "[native][arith]")
{
    const arith::outcome raised = arith::evaluate("3 ** 60");

    CHECK(raised.failed);
    REQUIRE(raised.messages.size() == 1);
    CHECK(raised.messages.front().find("integer power overflow") != std::string::npos);
    CHECK(arith::evaluate("2 ** 62").rendered == "4611686018427387904");
}

TEST_CASE("arithmetic inside the representable range is unchanged", "[native][arith]")
{
    CHECK(arith::evaluate("1 + 2").rendered == "3");
    CHECK(arith::evaluate("(1 + 2) * 3").rendered == "9");
    CHECK(arith::evaluate("-(4 - 1)").rendered == "-3");
    CHECK(arith::evaluate("-7 // 2").rendered == "-4");
    CHECK(arith::evaluate("-7 % 3").rendered == "2");
    CHECK(arith::evaluate("abs(-9)").rendered == "9");
    CHECK(arith::evaluate(most_negative).rendered == "-9223372036854775808");
    CHECK(arith::evaluate(std::string(most_negative) + " // 2").rendered == "-4611686018427387904");
}

TEST_CASE("a whole load carrying an overflowing product fails cleanly", "[native][arith]")
{
    const arith::loaded ran = arith::expand("9223372036854775807 * 2");

    CHECK_FALSE(ran.expanded);
    REQUIRE_FALSE(ran.messages.empty());
    CHECK(ran.messages.front().find(outside_range) != std::string::npos);
    CHECK(arith::expand("2 * 3").expanded);
}

TEST_CASE("rounding a value too large to be an integer refuses and names it", "[native][arith]")
{
    const arith::outcome rounded = arith::evaluate("floor(1e300)");

    CHECK(rounded.failed);
    REQUIRE(rounded.messages.size() == 1);
    CHECK(rounded.messages.front().find("1e+300") != std::string::npos);
    CHECK(rounded.messages.front().find(outside_range) != std::string::npos);
    CHECK(arith::evaluate("ceil(-1e300)").failed);
    CHECK(arith::evaluate("floor(9223372036854775808.0)").failed);
}

TEST_CASE("rounding inside the range is unchanged, extremes included", "[native][arith]")
{
    CHECK(arith::evaluate("floor(2.7)").rendered == "2");
    CHECK(arith::evaluate("ceil(2.1)").rendered == "3");
    CHECK(arith::evaluate("floor(-2.1)").rendered == "-3");
    CHECK(arith::evaluate("floor(9223372036854774784.0)").rendered == "9223372036854774784");
    CHECK(arith::evaluate("ceil(-9223372036854775808.0)").rendered == "-9223372036854775808");
}

// The inertia expression a real description writes spells its leading operand with a trailing
// point and raises with the exponentiation operator in the same term, so both are read here
// rather than assumed loadable.
TEST_CASE("a float literal written with a trailing point evaluates beside exponentiation",
          "[native][arith]")
{
    CHECK(arith::evaluate("1./12").rendered == "0.08333333333333333");
    CHECK(arith::evaluate("2.**3").rendered == "8.0");
    CHECK(arith::evaluate("1.e2").rendered == "100.0");
    CHECK(arith::evaluate("1./12 * 3.7 * (3 * 0.06**2 + 0.12**2)").rendered == "0.00777");
    CHECK(arith::evaluate(".5 + 1").rendered == "1.5");
}

TEST_CASE("two strings add where every other operator on a string still refuses",
          "[native][arith]")
{
    CHECK(arith::evaluate("'mesh.' + 'stl'").rendered == "mesh.stl");
    CHECK(arith::evaluate("'a' + '' + 'b'").rendered == "ab");
    CHECK(arith::evaluate("'x' + 1").failed);
    CHECK(arith::evaluate("'x' * 2").failed);
    CHECK(arith::evaluate("'x' - 'y'").failed);
    CHECK(arith::evaluate("-'x'").failed);
}

TEST_CASE("the exported printer spells every double a consumer can hand it", "[native][arith]")
{
    constexpr double infinite = std::numeric_limits<double>::infinity();

    CHECK(meios::detail::print_double(infinite) == "inf");
    CHECK(meios::detail::print_double(-infinite) == "-inf");
    CHECK(meios::detail::print_double(std::numeric_limits<double>::quiet_NaN()) == "nan");
    CHECK(meios::detail::print_double(-std::numeric_limits<double>::quiet_NaN()) == "nan");
    CHECK(meios::detail::print_double(1.5) == "1.5");
    CHECK(meios::detail::print_double(2.0) == "2.0");
}

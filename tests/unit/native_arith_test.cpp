#include "arith_probe.h"

#include <meios/xacro/detail/numeric.h>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <utility>
#include <string_view>

namespace
{
constexpr const char *most_negative = "(-9223372036854775807 - 1)";
}

TEST_CASE("a product past 64 bits is the exact integer instead of wrapping", "[native][arith]")
{
    CHECK(arith::evaluate("9223372036854775807 * 2").rendered == "18446744073709551614");
    CHECK(arith::evaluate("4611686018427387904 * -4").rendered == "-18446744073709551616");
}

TEST_CASE("the most negative integer over minus one gives the exact quotient and a zero remainder", "[native][arith]")
{
    CHECK(arith::evaluate(std::string(most_negative) + " // -1").rendered == "9223372036854775808");
    CHECK(arith::evaluate(std::string(most_negative) + " % -1").rendered == "0");
}

TEST_CASE("negating and taking the size of the most negative integer give the exact integer", "[native][arith]")
{
    CHECK(arith::evaluate(std::string("-") + most_negative).rendered == "9223372036854775808");
    CHECK(arith::evaluate(std::string("abs(") + most_negative + ")").rendered == "9223372036854775808");
    CHECK(arith::evaluate("-(9223372036854775807 + 1)").rendered == "-9223372036854775808");
}

TEST_CASE("adding and subtracting past either end give the exact integer", "[native][arith]")
{
    CHECK(arith::evaluate("9223372036854775807 + 1").rendered == "9223372036854775808");
    CHECK(arith::evaluate(std::string(most_negative) + " - 1").rendered == "-9223372036854775809");
    CHECK(arith::evaluate(std::string(most_negative) + " + -1").rendered == "-9223372036854775809");
    CHECK(arith::evaluate("9223372036854775807 - -1").rendered == "9223372036854775808");
}

TEST_CASE("exponentiation past 64 bits gives the exact integer", "[native][arith]")
{
    CHECK(arith::evaluate("3 ** 60").rendered == "42391158275216203514294433201");
    CHECK(arith::evaluate("2 ** 62").rendered == "4611686018427387904");
    CHECK(arith::evaluate("2 ** 63").rendered == "9223372036854775808");
    CHECK(arith::evaluate("-2 ** 63").rendered == "-9223372036854775808");
    CHECK(arith::evaluate("(-2) ** 63").rendered == "-9223372036854775808");
}

TEST_CASE("arithmetic inside the representable range is unchanged", "[native][arith]")
{
    CHECK(arith::evaluate("1 + 2").rendered == "3");
    CHECK(arith::evaluate("(1 + 2) * 3").rendered == "9");
    CHECK(arith::evaluate("-(4 - 1)").rendered == "-3");
    CHECK(arith::evaluate("-7 // 2").rendered == "-4");
    CHECK(arith::evaluate("-7 % 3").rendered == "2");
    CHECK(arith::evaluate("abs(-9)").rendered == "9");
    CHECK(arith::evaluate("2 ** -1").rendered == "0.5");
    CHECK(arith::evaluate(most_negative).rendered == "-9223372036854775808");
    CHECK(arith::evaluate(std::string(most_negative) + " // 2").rendered == "-4611686018427387904");
}

TEST_CASE("a whole load carrying a product past 64 bits renders the exact integer", "[native][arith]")
{
    const arith::loaded ran = arith::expand("9223372036854775807 * 2");

    REQUIRE(ran.expanded);
    CHECK(ran.text.find("radius=\"18446744073709551614\"") != std::string::npos);
    CHECK(arith::expand("2 * 3").expanded);
}

TEST_CASE("rounding answers an integer unchanged and a real as the exact integer it rounds to, of any size", "[native][arith]")
{
    CHECK(arith::evaluate("floor(99999999999999999999)").rendered == "99999999999999999999");
    CHECK(arith::evaluate("floor(9007199254740993)").rendered == "9007199254740993");
    CHECK(arith::evaluate("ceil(True)").rendered == "1");
    CHECK(arith::evaluate("floor(1e30)").rendered == "1000000000000000019884624838656");
    const std::string largest = arith::evaluate("floor(1e300)").rendered;
    CHECK(largest.size() == 301);
    CHECK(largest.starts_with("1000000000000000052504760255204420248704468581108159154915854115511802457988908195786371375080447864"));
    CHECK(arith::evaluate("ceil(-1e300)").rendered == "-" + largest);
    CHECK(arith::evaluate("floor(9223372036854775808.0)").rendered == "9223372036854775808");
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

TEST_CASE("a real floor division counts the whole divisors that fit, by every sign", "[native][arith]")
{
    const std::pair<std::string_view, std::string_view> quotients[] = {
            {"1 // 0.1", "9.0"}, {"-1 // 0.1", "-10.0"}, {"1 // -0.1", "-10.0"}, {"-1 // -0.1", "9.0"},
            {"530 // 0.1", "5299.0"}, {"-530 // -0.1", "5299.0"}, {"0.5 // 0.1", "4.0"}, {"2.0 // 0.2", "9.0"},
            {"(1 // 0.1) * 0.1 + 1 % 0.1", "1.0"}, {"7.5 // 2", "3.0"}, {"-7.5 // 2", "-4.0"},
            {"0.0 // 3.0", "0.0"}, {"-0.0 // 3.0", "-0.0"}, {"0 // -3.0", "-0.0"},
            {"2**64 // 0.1", "1.844674407370955e+20"}, {"-(2**64) // 0.1", "-1.844674407370955e+20"}};
    for(const auto &[expression, rendered] : quotients)
    {
        INFO(expression);
        CHECK(arith::evaluate(expression).rendered == rendered);
    }
    const arith::outcome past = arith::evaluate("1e308 // 1e-308");
    CHECK(past.failed);
    REQUIRE(past.messages.size() == 1);
    CHECK(past.messages.front().find("not a finite number") != std::string::npos);
}

TEST_CASE("a real remainder of exactly zero is written with the dividend's sign", "[native][arith]")
{
    CHECK(arith::evaluate("-4.0 % 2.0").rendered == "-0.0");
    CHECK(arith::evaluate("4.0 % -2.0").rendered == "0.0");
    CHECK(arith::evaluate("-4.0 % 2.0 == 0").rendered == "True");
}

TEST_CASE("a zero divisor refuses naming its operator", "[native][arith]")
{
    const std::pair<std::string_view, std::string_view> divisions[] = {
            {"1 / 0", "/"}, {"1.0 / 0", "/"}, {"7 // 0", "//"}, {"7.0 // 0", "//"}, {"5 // 0.0", "//"}, {"7 % 0", "%"}, {"7.0 % 0.0", "%"},
            {"99999999999999999999 / 0", "/"}, {"99999999999999999999 // False", "//"}, {"-99999999999999999999 % 0", "%"}};
    for(const auto &[expression, op] : divisions)
    {
        INFO(expression);
        const arith::outcome ran = arith::evaluate(expression);
        CHECK(ran.failed);
        REQUIRE(ran.messages.size() == 1);
        CHECK(ran.messages.front().find("the divisor of '" + std::string(op) + "' is zero") != std::string::npos);
    }
}

TEST_CASE("a refusal naming an operand's kind gives the kind its article", "[native][arith]")
{
    const std::pair<std::string_view, std::string_view> refused[] = {
            {"'x' + 1", "not with an integer"}, {"1 in 'abc'", "not an integer"}, {"'a,b'.split(1)", "not an integer"}, {"1[0]", "an integer cannot be subscripted here"}};
    for(const auto &[expression, fragment] : refused)
    {
        INFO(expression);
        const arith::outcome ran = arith::evaluate(expression);
        CHECK(ran.failed);
        REQUIRE(ran.messages.size() == 1);
        CHECK(ran.messages.front().find(fragment) != std::string::npos);
    }
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

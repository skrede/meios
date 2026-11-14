#include "meios/xacro/big_integer.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstdint>
#include <optional>
#include <string_view>

using meios::detail::big_integer;

namespace
{

big_integer big(std::string_view spelling)
{
    const std::optional<big_integer> read = big_integer::from_spelling(spelling);
    REQUIRE(read);
    return *read;
}

std::string spelled(const big_integer &number)
{
    return number.spelling();
}

constexpr std::int64_t lowest = -9223372036854775807 - 1;
constexpr std::int64_t highest = 9223372036854775807;
const std::string wide = "99999999999999999999";

}

TEST_CASE("an arbitrary-precision integer spells back the digits it was read from", "[native][big_integer]")
{
    const std::string longest(4300, '7');
    for(const std::string &text : { std::string("0"), std::string("1"), std::string("-1"), std::string("9223372036854775807"),
                                    std::string("-9223372036854775808"), std::string("18446744073709551616"), "-" + wide, longest })
    {
        CAPTURE(text);
        CHECK(spelled(big(text)) == text);
    }
    CHECK(spelled(big_integer{ lowest }) == "-9223372036854775808");
    CHECK(spelled(big_integer{ std::int64_t{ 0 } }) == "0");
    for(const char *text : { "", "-", "+5", "5a", "1.0" })
        CHECK_FALSE(big_integer::from_spelling(text));
}

TEST_CASE("an arbitrary-precision integer answers its 64-bit value only inside the range", "[native][big_integer]")
{
    CHECK(big("9223372036854775807").int64() == std::optional<std::int64_t>(highest));
    CHECK(big("-9223372036854775808").int64() == std::optional<std::int64_t>(lowest));
    CHECK(big("-0").int64() == std::optional<std::int64_t>(0));
    CHECK_FALSE(big("9223372036854775808").int64());
    CHECK_FALSE(big("-9223372036854775809").int64());
    CHECK_FALSE(big(wide).int64());
}

TEST_CASE("an arbitrary-precision integer knows its sign and bit length", "[native][big_integer]")
{
    CHECK_FALSE(big("0").negative());
    CHECK_FALSE(big("-0").negative());
    CHECK(big("-1").negative());
    CHECK(big("0").bit_length() == 0);
    CHECK(big("1").bit_length() == 1);
    CHECK(big("4294967296").bit_length() == 33);
    CHECK(big("-9223372036854775808").bit_length() == 64);
}

TEST_CASE("arbitrary-precision integers order by sign and then magnitude", "[native][big_integer]")
{
    CHECK(big("-" + wide) < big("-1"));
    CHECK(big("-1") < big("0"));
    CHECK(big("0") < big(wide));
    CHECK(big("-" + wide) < big("-99999999999999999998"));
    CHECK(big(wide) == big(wide));
    CHECK(big("-0") == big("0"));
    CHECK(big("18446744073709551616") > big("18446744073709551615"));
}

TEST_CASE("arbitrary-precision integers add and subtract across signs and limb carries", "[native][big_integer]")
{
    CHECK(spelled(big("4294967295") + big("1")) == "4294967296");
    CHECK(spelled(big("18446744073709551615") + big("1")) == "18446744073709551616");
    CHECK(spelled(big("18446744073709551616") - big("1")) == "18446744073709551615");
    CHECK(spelled(big(wide) - big(wide)) == "0");
    CHECK_FALSE((big(wide) - big(wide)).negative());
    CHECK(spelled(big("-" + wide) + big(wide)) == "0");
    CHECK(spelled(big("5") - big("7")) == "-2");
    CHECK(spelled(big("-5") + big("7")) == "2");
    CHECK(spelled(big("-5") - big("7")) == "-12");
    CHECK(spelled(big(wide) + big("1")) == "100000000000000000000");
    CHECK(spelled(big("9223372036854775807") + big("1")) == "9223372036854775808");
    CHECK(spelled(big("-9223372036854775808") - big("1")) == "-9223372036854775809");
}

TEST_CASE("an arbitrary-precision integer negates and takes its absolute value", "[native][big_integer]")
{
    CHECK(spelled(-big_integer{ lowest }) == "9223372036854775808");
    CHECK(spelled(-big(wide)) == "-" + wide);
    CHECK(spelled(-big("0")) == "0");
    CHECK_FALSE((-big("0")).negative());
    CHECK(spelled(abs(big("-" + wide))) == wide);
    CHECK(spelled(abs(big(wide))) == wide);
}

TEST_CASE("arbitrary-precision integers multiply across limbs and signs", "[native][big_integer]")
{
    CHECK(spelled(big("4294967296") * big("4294967296")) == "18446744073709551616");
    CHECK(spelled(big("18446744073709551615") * big("18446744073709551615")) == "340282366920938463426481119284349108225");
    CHECK(spelled(big("-3") * big(wide)) == "-299999999999999999997");
    CHECK(spelled(big("-" + wide) * big("-" + wide)) == "9999999999999999999800000000000000000001");
    CHECK(spelled(big("-" + wide) * big("0")) == "0");
    CHECK_FALSE((big("-" + wide) * big("0")).negative());
}

TEST_CASE("an arbitrary-precision integer raises to a non-negative power", "[native][big_integer]")
{
    CHECK(spelled(pow(big("3"), 60)) == "42391158275216203514294433201");
    CHECK(spelled(pow(big("2"), 70)) == "1180591620717411303424");
    CHECK(spelled(pow(big("-2"), 63)) == "-9223372036854775808");
    CHECK(pow(big("-2"), 63).int64() == std::optional<std::int64_t>(lowest));
    CHECK(spelled(pow(big(wide), 2)) == "9999999999999999999800000000000000000001");
    for(const char *base : { "0", "1", "-7", "99999999999999999999" })
        CHECK(spelled(pow(big(base), 0)) == "1");
    CHECK(spelled(pow(big("0"), 5)) == "0");
}

namespace
{

void check_division(std::string_view dividend, std::string_view divisor, std::string_view quotient, std::string_view remainder)
{
    CAPTURE(dividend, divisor);
    const auto [q, r] = floor_divmod(big(dividend), big(divisor));
    CHECK(spelled(q) == quotient);
    CHECK(spelled(r) == remainder);
    CHECK(q * big(divisor) + r == big(dividend));
    CHECK(abs(r) < abs(big(divisor)));
    CHECK((r == big("0") || r.negative() == big(divisor).negative()));
}

}

TEST_CASE("arbitrary-precision floor division rounds toward negative infinity over every sign pair", "[native][big_integer]")
{
    const std::string n = "1000000000000000000000000000000";
    check_division(n, "7", "142857142857142857142857142857", "1");
    check_division("-" + n, "7", "-142857142857142857142857142858", "6");
    check_division(n, "-7", "-142857142857142857142857142858", "-6");
    check_division("-" + n, "-7", "142857142857142857142857142857", "-1");
    check_division("-9223372036854775808", "-1", "9223372036854775808", "0");
    check_division("5", "1180591620717411303424", "0", "5");
    check_division("-5", "1180591620717411303424", "-1", "1180591620717411303419");
    check_division("0", "-" + wide, "0", "0");
}

TEST_CASE("arbitrary-precision floor division over a divisor of several limbs", "[native][big_integer]")
{
    const std::string n = "340282366920938463463374607431768211461", d = "18446744073709551619";
    check_division(n, d, "18446744073709551613", "14");
    check_division("-" + n, d, "-18446744073709551614", "18446744073709551605");
    check_division(n, "-" + d, "-18446744073709551614", "-18446744073709551605");
    check_division("-" + n, "-" + d, "18446744073709551613", "-14");
}

TEST_CASE("arbitrary-precision floor division adds the divisor back when an estimate is one too large", "[native][big_integer]")
{
    const std::string n = "79228162514264337593543950336", d = "18446744073709551617";
    check_division(n, d, "4294967295", "18446744069414584321");
    check_division("-" + n, d, "-4294967296", "4294967296");
    check_division(n, "-" + d, "-4294967296", "-4294967296");
}

TEST_CASE("arbitrary-precision floor division corrects an estimate twice by the divisor's second limb", "[native][big_integer]")
{
    check_division("604444463063240877801472", "140739635838975", "4294770690", "140737488158722");
    check_division("-604444463063240877801472", "140739635838975", "-4294770691", "2147680253");
    const std::string n = "2596069201104899549927654620659712", d = "604472133179347147161600";
    check_division(n, d, "4294770690", "604462908962898247155712");
    check_division(n, "-" + d, "-4294770691", "-9224216448900005888");
}

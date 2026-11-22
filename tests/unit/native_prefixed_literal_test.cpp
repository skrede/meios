#include "syntax_probe.h"

#include "meios/xacro/big_integer.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

using meios::detail::big_integer;

namespace
{

struct literal_case
{
    std::string_view spelling;
    std::string_view expected;
};

meios::expected<meios::expansion, meios::expansion_error> strictly(std::string_view expression)
{
    return syntax_probe::load_under("${" + std::string(expression) + "}", meios::eval_policy::fail);
}

meios::value evaluated(std::string_view expression)
{
    const meios::eval_scope scope;
    meios::log_sink silent;
    meios::core_evaluator evaluator;
    return evaluator.eval(expression, scope, silent);
}

std::string ones_after(std::string_view prefix, char digit, std::size_t count)
{
    return std::string(prefix) + std::string(count, digit);
}

}

TEST_CASE("a prefixed literal wider than 64 bits is a wide integer", "[native][syntax]")
{
    for(const literal_case one :
        {literal_case{"0x1_0000_0000_0000_0000 + 1", "18446744073709551617"}, {"0xffff_ffff_ffff_ffff", "18446744073709551615"}, {"-0o7777777777777777777777", "-73786976294838206463"}})
    {
        INFO("literal: " << one.spelling);
        CHECK(probe::evaluate(one.spelling).rendered == one.expected);
    }
    CHECK(probe::evaluate(ones_after("0b1", '0', 64)).rendered == "18446744073709551616");
    const meios::value widest_small = evaluated("0x7fff_ffff_ffff_ffff");
    CHECK(widest_small.kind() == meios::value_kind::integer);
    CHECK(widest_small.integer() == 9223372036854775807);
}

TEST_CASE("a prefixed literal whose value is past the digit ceiling refuses", "[native][syntax]")
{
    CHECK((pow(big_integer(2), 14285) - big_integer(1)).spelling().size() == 4301);
    for(const std::string &literal : {ones_after("0x", 'f', 3600), ones_after("0x1", 'f', 3571)})
    {
        INFO("hexadecimal digits: " << literal.size() - 2);
        const auto expanded = strictly(literal);
        REQUIRE_FALSE(expanded.has_value());
        CHECK(expanded.error().code == meios::diagnostic_code::expression_error);
        CHECK(expanded.error().message.find("4300") != std::string::npos);
    }
    const std::string within = (pow(big_integer(16), 3500) - big_integer(1)).spelling();
    CHECK(probe::evaluate(ones_after("0x", 'f', 3500)).rendered == within);
}

TEST_CASE("base-two digits pack into an arbitrary-precision integer", "[native][big_integer]")
{
    CHECK(big_integer::from_power_of_two_digits("0", 4) == big_integer(0));
    CHECK(big_integer::from_power_of_two_digits("000", 1) == big_integer(0));
    CHECK(big_integer::from_power_of_two_digits("ff", 4) == big_integer(255));
    CHECK(big_integer::from_power_of_two_digits("777", 3) == big_integer(511));
    CHECK(big_integer::from_power_of_two_digits("7777777777777777777777", 3) == pow(big_integer(2), 66) - big_integer(1));
    CHECK_FALSE(big_integer::from_power_of_two_digits("2", 1).has_value());
}

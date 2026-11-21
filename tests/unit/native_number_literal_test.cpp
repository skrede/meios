#include "syntax_probe.h"

#include "meios/xacro/big_integer.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <utility>
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

TEST_CASE("an underscored, prefixed or pointed literal evaluates to its number", "[native][syntax]")
{
    for(const literal_case one : { literal_case{ "0_0", "0" }, { "00_0", "0" }, { "1_000", "1000" }, { "1_000_000", "1000000" },
                                   { "0x10", "16" }, { "0X1_f", "31" }, { "0x_1f", "31" }, { "0o17", "15" }, { "0O1_7", "15" },
                                   { "0b101", "5" }, { "0B1_0", "2" }, { "-0x10", "-16" }, { "0x1F + 1", "32" },
                                   { "1_0.5", "10.5" }, { ".5_0", "0.5" }, { "0_7.5", "7.5" }, { "07.5", "7.5" },
                                   { "0_7e1", "70.0" }, { "1_0e1_0", "100000000000.0" }, { "1.e5", "100000.0" } })
    {
        INFO("literal: " << one.spelling);
        CHECK(probe::evaluate(one.spelling).rendered == one.expected);
    }
}

TEST_CASE("a prefixed literal wider than 64 bits is a wide integer", "[native][syntax]")
{
    for(const literal_case one : { literal_case{ "0x1_0000_0000_0000_0000 + 1", "18446744073709551617" },
                                   { "0xffff_ffff_ffff_ffff", "18446744073709551615" },
                                   { "-0o7777777777777777777777", "-73786976294838206463" } })
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
    for(const std::string &literal : { ones_after("0x", 'f', 3600), ones_after("0x1", 'f', 3571) })
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

TEST_CASE("a malformed literal refuses as an expression fault naming the fault", "[native][syntax]")
{
    const std::string_view leading = "leading zero", separator = "digit separator", bare = "followed by no digit",
                           outside = "holds only the digits", exponent = "exponent", glued = "directly followed by a name",
                           range = "outside the range of a double";
    for(const literal_case one : { literal_case{ "0_7", leading }, { "00_7", leading }, { "07", leading },
                                   { "1__0", separator }, { "1_", separator }, { "0_", separator }, { "0__0", separator },
                                   { "1_.5", separator }, { "1._5", separator }, { "1e_5", separator }, { "1e5_", separator },
                                   { "1_e5", separator }, { "1e+", exponent }, { "0x", bare }, { "0x_", bare },
                                   { "0x1_", separator }, { "0x__1", separator }, { "0o", bare }, { "0b", bare },
                                   { "0b_", bare }, { "0b2", outside }, { "0o8", outside }, { "0b12", outside },
                                   { "0or 2", bare }, { "1e", glued }, { "1abc", glued }, { "1jx", glued },
                                   { "0x1j", glued }, { "1.5abc", glued }, { "1e999", range } })
    {
        INFO("literal: " << one.spelling);
        const auto expanded = strictly(one.spelling);
        REQUIRE_FALSE(expanded.has_value());
        CHECK(expanded.error().code == meios::diagnostic_code::expression_error);
        CHECK(expanded.error().message.find(one.expected) != std::string::npos);
    }
}

TEST_CASE("a number beside a name or a number still refuses as an expression fault", "[native][syntax]")
{
    for(std::string_view literal : { "0x1.5", "0xg", "0x1j" })
    {
        INFO("literal: " << literal);
        const auto expanded = strictly(literal);
        REQUIRE_FALSE(expanded.has_value());
        CHECK(expanded.error().code == meios::diagnostic_code::expression_error);
    }
}

TEST_CASE("an imaginary literal refuses as unsupported, naming a complex number", "[native][syntax]")
{
    for(std::string_view literal : { "07j", "0_7j", "1_0j", "1.5j", "1e3j", "0j", ".5j" })
    {
        INFO("literal: " << literal);
        const probe::outcome ran = probe::evaluate(literal);
        CHECK(ran.failed);
        CHECK(ran.kind == meios::eval_failure_kind::unsupported);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find("complex number") != std::string::npos);
        const auto expanded = strictly(literal);
        REQUIRE_FALSE(expanded.has_value());
        CHECK(expanded.error().code == meios::diagnostic_code::unsupported_expression);
    }
}

TEST_CASE("a lenient policy leaves an imaginary literal's span as written", "[native][syntax]")
{
    for(meios::eval_policy policy : { meios::eval_policy::skip, meios::eval_policy::warn })
    {
        const auto expanded = syntax_probe::load_under("<m v=\"${07j}\"/>", policy);
        REQUIRE(expanded.has_value());
        CHECK(expanded->document.find("v=\"${07j}\"") != std::string::npos);
    }
    CHECK_FALSE(syntax_probe::loads("<m v=\"${07j}\"/>", meios::eval_policy::fail));
}

TEST_CASE("a number directly followed by a keyword still reads as the number and the keyword", "[native][syntax]")
{
    for(const literal_case one : { literal_case{ "1 if True else 2", "1" }, { "1if True else 2", "1" },
                                   { "1if 1else 2", "1" }, { "1and 0", "0" },
                                   { "0b1and 2", "2" }, { "1 if 0else 2", "2" }, { "True or 1", "True" }, { "not 1in cfg", "True" } })
    {
        INFO("expression: " << one.spelling);
        CHECK(probe::evaluate(one.spelling).rendered == one.expected);
    }
}

TEST_CASE("a property's prefixed or underscored text keeps its literal reading", "[native][syntax]")
{
    for(std::string_view text : { "0x10", "1_0" })
    {
        INFO("value: " << text);
        const std::string body = "<xacro:property name=\"p\" value=\"" + std::string(text) + "\"/><m v=\"${p}\"/>";
        const auto expanded = syntax_probe::load_under(body, meios::eval_policy::fail);
        REQUIRE(expanded.has_value());
        CHECK(expanded->document.find("v=\"" + std::string(text) + "\"") != std::string::npos);
    }
    const auto leading = strictly("007");
    REQUIRE_FALSE(leading.has_value());
    CHECK(leading.error().message.find("leading zero") != std::string::npos);
}

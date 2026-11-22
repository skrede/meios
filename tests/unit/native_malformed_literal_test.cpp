#include "syntax_probe.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace
{

struct literal_case
{
    std::string_view spelling;
    std::string_view expected;
};

constexpr std::string_view leading = "leading zero", separator = "digit separator", bare = "followed by no digit", outside = "holds only the digits", exponent = "exponent",
                           glued = "directly followed by a name or a digit", too_large = "too large for a double";

constexpr literal_case malformed_literals[] = {
        {"0_7", leading},  {"00_7", leading},   {"07", leading},     {"07abc", leading},   {"0_7abc", leading},  {"07e", leading},    {"1__0", separator}, {"1_", separator},
        {"0_", separator}, {"0__0", separator}, {"1_.5", separator}, {"1._5", separator},  {"1e_5", separator},  {"1e5_", separator}, {"1_e5", separator}, {"1e+", exponent},
        {"0x", bare},      {"0x_", bare},       {"0x1_", separator}, {"0x__1", separator}, {"0o", bare},         {"0b", bare},        {"0b_", bare},       {"0b2", outside},
        {"0o8", outside},  {"0b12", outside},   {"0or 2", bare},     {"1e", glued},        {"1abc", glued},      {"1jx", glued},      {"0x1j", glued},     {"1.5abc", glued},
        {"1j2", glued},    {"00abc", glued},    {"07.5abc", glued},  {"1e309", too_large}, {"1e999", too_large},
};

meios::expected<meios::expansion, meios::expansion_error> strictly(std::string_view expression)
{
    return syntax_probe::load_under("${" + std::string(expression) + "}", meios::eval_policy::fail);
}

}

TEST_CASE("a malformed literal refuses as an expression fault naming the fault", "[native][syntax]")
{
    for(const literal_case one : malformed_literals)
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
    for(std::string_view literal : {"0x1.5", "0xg", "0x1j"})
    {
        INFO("literal: " << literal);
        const auto expanded = strictly(literal);
        REQUIRE_FALSE(expanded.has_value());
        CHECK(expanded.error().code == meios::diagnostic_code::expression_error);
    }
}

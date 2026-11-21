#include "oracle_records.h"

#include <meios/xacro/detail/numeric.h>

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <array>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <algorithm>
#include <string_view>

namespace
{

using meios::detail::notation;

constexpr std::string_view record = "rendering.cases";

std::vector<oracle::row> recorded_renderings()
{
    std::vector<oracle::row> rows = oracle::load_rows(record);
    REQUIRE_FALSE(rows.empty());
    for(const oracle::row &one : rows)
        REQUIRE(one.fields.size() >= 2);
    return rows;
}

double probe_value(const std::string &probe)
{
    bool ok = false;
    const double number = meios::detail::parse_double(probe, ok);
    REQUIRE(ok);
    return number;
}

std::uint64_t bits_of(double number)
{
    return std::bit_cast<std::uint64_t>(number);
}

// The probe's own decimal exponent, read from a "%.17e" form so it comes from the
// recorded text rather than from the code under test.
std::int32_t decade_of(const std::string &probe)
{
    std::array<char, 64> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.17e", probe_value(probe));
    const std::string text(buffer.data());
    return std::atoi(text.c_str() + text.find('e') + 1);
}

notation notation_of(const std::string &rendering)
{
    return rendering.find('e') == std::string::npos ? notation::fixed : notation::scientific;
}

std::pair<std::int32_t, std::int32_t> recorded_fixed_window(const std::vector<oracle::row> &rows)
{
    std::vector<std::int32_t> decades;
    for(const oracle::row &one : rows)
        if(notation_of(one.fields[1]) == notation::fixed)
            decades.push_back(decade_of(one.fields.front()));
    REQUIRE_FALSE(decades.empty());
    return { *std::min_element(decades.begin(), decades.end()),
             *std::max_element(decades.begin(), decades.end()) };
}

}

TEST_CASE("every recorded probe renders the text upstream renders", "[render]")
{
    for(const oracle::row &one : recorded_renderings())
    {
        INFO("probe " << one.fields.front());
        REQUIRE(meios::detail::print_double(probe_value(one.fields.front())) == one.fields[1]);
    }
}

TEST_CASE("every rendering parses back to the double that produced it", "[render]")
{
    for(const oracle::row &one : recorded_renderings())
    {
        INFO("probe " << one.fields.front());
        const double number = probe_value(one.fields.front());
        REQUIRE(bits_of(probe_value(meios::detail::print_double(number))) == bits_of(number));
    }
}

// Asserted against the rule itself rather than only through a rendering, because just one
// of the two toolchain spellings of the shortest round-trip is compiled on any one
// machine while the rule they share has to hold on both.
TEST_CASE("upstream picks notation by decimal exponent, not by the shorter spelling", "[render]")
{
    const std::vector<oracle::row> rows = recorded_renderings();
    for(const oracle::row &one : rows)
    {
        INFO("probe " << one.fields.front());
        REQUIRE(meios::detail::choose_notation(decade_of(one.fields.front()))
                == notation_of(one.fields[1]));
    }

    const auto [lowest, highest] = recorded_fixed_window(rows);
    REQUIRE(meios::detail::choose_notation(lowest) == notation::fixed);
    REQUIRE(meios::detail::choose_notation(lowest - 1) == notation::scientific);
    REQUIRE(meios::detail::choose_notation(highest) == notation::fixed);
    REQUIRE(meios::detail::choose_notation(highest + 1) == notation::scientific);
}

// Each verdict is the one std::from_chars gives, which the strtod reader compiled where floating-point
// charconv is missing must repeat: strtod alone reads a leading space, a plus sign and a hexadecimal
// form, and its ERANGE flags an in-range subnormal.
TEST_CASE("a real is read exactly where from_chars reads one", "[numeric]")
{
    for(const std::string_view refused : {"", "-", "+7", " 7", "\t1", "7 ", "0x10", "-0x1p3", "+inf", "1e309", "-1e309",
                                           "1.7976931348623159e308", "1e-400", "-2.4703282292062327e-324"})
    {
        INFO("spelling '" << refused << "'");
        bool ok = true;
        meios::detail::parse_double(refused, ok);
        CHECK_FALSE(ok);
    }
    constexpr double tiniest = std::numeric_limits<double>::denorm_min();
    const std::pair<std::string_view, double> readings[] = {
            {"-7", -7.0}, {"1e-308", 1e-308}, {"4.9e-324", tiniest}, {"-2.4703282292062328e-324", -tiniest},
            {"1.7976931348623158e308", std::numeric_limits<double>::max()}, {"0e-999", 0.0}, {"-0", -0.0},
            {"-inf", -std::numeric_limits<double>::infinity()}};
    for(const auto &[spelling, number] : readings)
    {
        INFO("spelling '" << spelling << "'");
        bool ok = false;
        CHECK(bits_of(meios::detail::parse_double(spelling, ok)) == bits_of(number));
        CHECK(ok);
    }
}

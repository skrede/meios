#include "oracle_records.h"

#include <meios/xacro/detail/numeric.h>

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <array>
#include <cstdio>
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

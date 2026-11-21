#include <meios/xacro/detail/numeric.h>

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cmath>
#include <string>
#include <cstdint>
#include <string_view>

namespace
{

struct pinned_power
{
    std::int32_t     exponent;
    std::string_view text;
};

// Python's repr of 2.0**k at the powers of two whose shortest spelling is not the correctly
// rounded one at its length but that spelling's neighbor one unit of the last digit away.
constexpr pinned_power pinned[] = {
    { -1017, "7.120236347223045e-307" }, { -1007, "7.291122019556398e-304" },
    { -957, "8.209073602596753e-289" },  { -921, "5.641232424577593e-278" },
    { -808, "5.858190679279809e-244" },  { -791, "7.678447687145631e-239" },
    { -788, "6.142758149716505e-238" },  { -778, "6.290184345309701e-235" },
    { -705, "5.940911144672375e-213" },  { -695, "6.083493012144512e-210" },
    { -662, "5.225680706521042e-200" },  { -652, "5.351097043477547e-197" },
    { -549, "5.426657103235053e-166" },  { -509, "5.966672584960166e-154" },
    { -496, "4.887898181599368e-150" },  { -489, "6.256509672447191e-148" },
    { -383, "5.075883674631299e-116" },  { -366, "6.653062250012736e-111" },
    { -296, "7.854549544476363e-90" },   { -140, "7.174648137343064e-43" },
    { -97, "6.310887241768095e-30" },    { -77, "6.617444900424222e-24" },
    { -44, "5.684341886080802e-14" },    { -24, "5.960464477539063e-08" },
    { 89, "6.189700196426902e+26" },     { 122, "5.316911983139664e+36" },
    { 132, "5.444517870735016e+39" },    { 172, "5.986310706507379e+51" },
    { 182, "6.129982163463556e+54" },    { 275, "6.070840288205404e+82" },
    { 305, "6.518515124270356e+91" },    { 345, "7.167183174968974e+103" },
    { 378, "6.156563468186638e+113" },   { 398, "6.455624695217272e+119" },
    { 405, "8.263199609878108e+121" },   { 481, "6.243497100631985e+144" },
    { 534, "5.623642243178996e+160" },   { 544, "5.758609657015292e+163" },
    { 554, "5.896816288783659e+166" },   { 574, "6.183260036827614e+172" },
    { 594, "6.483618076376552e+178" },   { 710, "5.386379163185535e+213" },
    { 803, "5.334411546303884e+241" },   { 863, "6.150157786156811e+259" },
    { 896, "5.282945311356653e+269" },   { 976, "6.386688990511104e+293" }
};

std::uint64_t bits_of(double number)
{
    return std::bit_cast<std::uint64_t>(number);
}

}

TEST_CASE("a power of two whose shortest spelling is a rounding neighbor prints as Python prints it",
          "[render]")
{
    for(const pinned_power &one : pinned)
    {
        INFO("2**" << one.exponent);
        CHECK(meios::detail::print_double(std::ldexp(1.0, one.exponent)) == one.text);
    }
}

TEST_CASE("every finite power of two reads back from the spelling it prints", "[render]")
{
    for(std::int32_t k = -1074; k <= 1023; ++k)
    {
        const double power = std::ldexp(1.0, k);
        const std::string text = meios::detail::print_double(power);
        INFO("2**" << k << " printed '" << text << "'");
        bool ok = false;
        CHECK(bits_of(meios::detail::parse_double(text, ok)) == bits_of(power));
        CHECK(ok);
    }
}

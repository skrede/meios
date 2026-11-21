#include "numeric_detail.h"

#include "meios/xacro/detail/numeric.h"

#include <array>
#include <cmath>
#include <string>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <charconv>
#include <system_error>

namespace meios::detail
{

// A value is d.ddd x 10^E, so the fixed spelling carrying the same significand has
// (significant - 1) - E digits after the point, and none at all once E overtakes them.
std::int32_t fixed_fraction(std::int32_t significant, std::int32_t exponent)
{
    const std::int32_t fraction = significant - 1 - exponent;
    return fraction < 0 ? 0 : fraction;
}

// Notation is picked from the decimal exponent alone rather than from whichever spelling is
// shorter, the rule of Python's float repr, so a document renders the text xacro renders: fixed
// while the exponent stays inside the window below, scientific outside it. The window is the
// one the recorded xacro renderings show -- 1e-4 and 1e15 print fixed, 1e-5 and 1e16 print
// scientific -- and a shorter-wins rule would emit "-5e-04" where xacro emits "-0.0005".
notation choose_notation(std::int32_t exponent)
{
    constexpr std::int32_t smallest_fixed = -4;
    constexpr std::int32_t largest_fixed = 15;
    if(exponent < smallest_fixed || exponent > largest_fixed)
        return notation::scientific;
    return notation::fixed;
}

#if MEIOS_HAS_FLOAT_CHARCONV

namespace
{

std::int32_t significant_digits(const std::string &scientific, std::size_t e_pos)
{
    std::int32_t digits = 0;
    for(std::size_t at = 0; at < e_pos; ++at)
        digits += (scientific[at] >= '0' && scientific[at] <= '9') ? 1 : 0;
    return digits;
}

}

// Shortest round-trip double->string, spelled in the chosen notation. to_chars' general
// form is unusable here because it applies its own shorter-wins rule, so the significand
// is taken in scientific form and respelled fixed when the notation asks for it.
std::string print_finite_shortest(double number)
{
    std::array<char, 64> buffer{};
    char *last = buffer.data() + buffer.size();
    std::to_chars_result made =
        std::to_chars(buffer.data(), last, number, std::chars_format::scientific);
    const std::string scientific(buffer.data(), made.ptr);

    const std::size_t e_pos = scientific.find('e');
    const std::int32_t exponent = std::atoi(scientific.c_str() + e_pos + 1);
    if(choose_notation(exponent) == notation::scientific)
        return scientific;

    const std::int32_t fraction = fixed_fraction(significant_digits(scientific, e_pos), exponent);
    made = std::to_chars(buffer.data(), last, number, std::chars_format::fixed, fraction);
    return std::string(buffer.data(), made.ptr);
}

#endif

// The value type refuses to construct a non-finite double, so nothing inside the evaluator
// arrives with one; print_double is re-exported through the umbrella header and a consumer
// can hand one in directly, so the entry point has to be total on its own. Both spellings
// below assume a decimal exponent is present, which is why the guard sits above them rather
// than inside either. The texts are the ones xacro renders, and what the fallback produced.
std::string print_double_shortest(double number)
{
    if(std::isfinite(number))
        return print_finite_shortest(number);
    if(std::isnan(number))
        return "nan";
    return number < 0.0 ? "-inf" : "inf";
}

// The largest real has 309 digits; at precision zero both spellings print the exact value.
std::string integral_digits(double number)
{
    std::array<char, 330> buffer{};
#if MEIOS_HAS_FLOAT_CHARCONV
    const std::to_chars_result made =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), number, std::chars_format::fixed, 0);
    return std::string(buffer.data(), made.ptr);
#else
    const int written = snprintf_c_locale(buffer.data(), buffer.size(), false, 0, number);
    return std::string(buffer.data(), buffer.data() + written);
#endif
}

}

#include "numeric_detail.h"

#include "meios/xacro/detail/numeric.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <charconv>
#include <system_error>

namespace meios::detail
{

namespace
{

// A value is d.ddd x 10^E, so the fixed spelling carrying the same significand has
// (significant - 1) - E digits after the point, and none at all once E overtakes them.
std::int32_t fixed_fraction(std::int32_t significant, std::int32_t exponent)
{
    const std::int32_t fraction = significant - 1 - exponent;
    return fraction < 0 ? 0 : fraction;
}

}

// Upstream renders a double with CPython's float repr, which picks notation from the
// decimal exponent alone rather than from whichever spelling is shorter: fixed while the
// exponent stays inside the window below, scientific outside it. The window is the one
// the recorded upstream renderings show -- 1e-4 and 1e15 print fixed, 1e-5 and 1e16 print
// scientific -- and a shorter-wins rule would emit "-5e-04" where upstream emits
// "-0.0005".
notation choose_notation(std::int32_t exponent)
{
    constexpr std::int32_t smallest_fixed = -4;
    constexpr std::int32_t largest_fixed = 15;
    if(exponent < smallest_fixed || exponent > largest_fixed)
        return notation::scientific;
    return notation::fixed;
}

#if !MEIOS_HAS_FLOAT_CHARCONV

// snprintf under an isolated "C" locale -- the format-side twin of strtod_c_locale --
// so a comma-decimal global locale cannot leak a "," decimal separator.
int snprintf_c_locale(char *buffer, std::size_t size, bool scientific, int precision, double number)
{
    #if defined(_WIN32)
    static _locale_t c_locale = _create_locale(LC_ALL, "C");
    return scientific ? _snprintf_l(buffer, size, "%.*e", c_locale, precision, number)
                      : _snprintf_l(buffer, size, "%.*f", c_locale, precision, number);
    #else
    static locale_t c_locale = newlocale(LC_ALL_MASK, "C", static_cast<locale_t>(0));
    locale_t previous = uselocale(c_locale);
    int written = scientific ? std::snprintf(buffer, size, "%.*e", precision, number)
                             : std::snprintf(buffer, size, "%.*f", precision, number);
    uselocale(previous);
    return written;
    #endif
}

namespace
{

// Strip trailing zeros (and a bare '.') from a "%e" mantissa.
std::string trim_mantissa(const std::string &scientific, std::size_t e_pos)
{
    std::string mantissa = scientific.substr(0, e_pos);
    if(mantissa.find('.') != std::string::npos)
    {
        std::size_t last = mantissa.find_last_not_of('0');
        if(mantissa[last] == '.')
            --last;
        mantissa.erase(last + 1);
    }
    return mantissa + scientific.substr(e_pos);
}

}

// Shortest round-trip double->string for toolchains (libc++) lacking float to_chars.
// Finds the fewest significant digits whose scientific form parses bit-exactly back
// through strtod_c_locale -- that significand is the shortest round-trip -- and spells it
// in the chosen notation. "%.17e" always round-trips a finite double, so the loop
// terminates. Only finite values reach here; the entry point below holds the guard.
std::string print_finite_shortest(double number)
{
    std::array<char, 64> buffer{};
    for(int sig = 1; sig <= 17; ++sig)
    {
        int written = snprintf_c_locale(buffer.data(), buffer.size(), true, sig - 1, number);
        std::string scientific(buffer.data(), buffer.data() + written);
        char *stop = nullptr;
        if(strtod_c_locale(scientific.c_str(), &stop) != number)
            continue;

        // Decimal exponent E: value == d.ddd x 10^E, read from the %e mantissa.
        const std::size_t e_pos = scientific.find('e');
        const std::int32_t exponent = std::atoi(scientific.c_str() + e_pos + 1);
        if(choose_notation(exponent) == notation::scientific)
            return trim_mantissa(scientific, e_pos);

        written = snprintf_c_locale(buffer.data(), buffer.size(), false,
                                    fixed_fraction(sig, exponent), number);
        return std::string(buffer.data(), buffer.data() + written);
    }
    int written = snprintf_c_locale(buffer.data(), buffer.size(), true, 16, number);
    return std::string(buffer.data(), buffer.data() + written);
}

#else

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
// than inside either. The texts are upstream's, and what the fallback already produced.
std::string print_double_shortest(double number)
{
    if(std::isfinite(number))
        return print_finite_shortest(number);
    if(std::isnan(number))
        return "nan";
    return number < 0.0 ? "-inf" : "inf";
}

}

#include "meios/xacro/detail/numeric.h"

#include <array>
#include <cstdio>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <charconv>
#include <version>
#include <system_error>

#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    #define MEIOS_HAS_FLOAT_CHARCONV 1
#else
    #define MEIOS_HAS_FLOAT_CHARCONV 0
    #if defined(_WIN32)
        #include <clocale>
    #else
        #include <locale.h>
        #if defined(__APPLE__)
            // Under a strict -std (CMAKE_CXX_EXTENSIONS OFF -> __STRICT_ANSI__) some
            // macOS SDKs hide the POSIX xlocale extensions (locale_t, newlocale,
            // uselocale, LC_ALL_MASK) from <locale.h>; <xlocale.h> declares them
            // unconditionally. Harmless where <locale.h> already exposes them.
            #include <xlocale.h>
        #endif
    #endif
#endif

namespace meios::detail
{

#if !MEIOS_HAS_FLOAT_CHARCONV

// Fallback for toolchains (historically libc++) lacking float from_chars/to_chars.
// strtod/snprintf run under an explicit "C" locale so a comma-decimal global
// locale cannot corrupt URDF "."-decimals; charconv above is already locale-free.
double strtod_c_locale(const char *first, char **last)
{
    #if defined(_WIN32)
    static _locale_t c_locale = _create_locale(LC_ALL, "C");
    return _strtod_l(first, last, c_locale);
    #else
    static locale_t c_locale = newlocale(LC_ALL_MASK, "C", static_cast<locale_t>(0));
    locale_t previous = uselocale(c_locale);
    double number = std::strtod(first, last);
    uselocale(previous);
    return number;
    #endif
}

// snprintf under an isolated "C" locale -- the format-side twin of strtod_c_locale
// above -- so a comma-decimal global locale cannot leak a "," decimal separator.
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

// Shortest round-trip double->string, reproducing std::to_chars(double)'s general
// form for toolchains (libc++) lacking float to_chars. First finds the fewest
// significant digits whose scientific form parses bit-exactly back through
// strtod_c_locale -- that significand is the shortest round-trip. Then emits the
// notation to_chars would pick: the shorter of fixed and scientific, fixed
// preferred on a tie (not %g's exponent-threshold rule, which would render 180.0
// as "1.8e+02"). "%.17e" always round-trips a finite double, so the loop
// terminates; non-finite values (isfinite-gated upstream) fall through defensively.
std::string print_double_shortest(double number)
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
        std::size_t e_pos = scientific.find('e');
        int exponent = std::atoi(scientific.c_str() + e_pos + 1);
        // Strip trailing zeros (and a bare '.') from the scientific mantissa.
        std::string mantissa = scientific.substr(0, e_pos);
        if(mantissa.find('.') != std::string::npos)
        {
            std::size_t last = mantissa.find_last_not_of('0');
            if(mantissa[last] == '.')
                --last;
            mantissa.erase(last + 1);
        }
        scientific = mantissa + scientific.substr(e_pos);

        // Fixed form: fractional digits = sig - 1 - E, clamped at zero.
        int fraction = sig - 1 - exponent;
        if(fraction < 0)
            fraction = 0;
        written = snprintf_c_locale(buffer.data(), buffer.size(), false, fraction, number);
        std::string fixed(buffer.data(), buffer.data() + written);

        return fixed.size() <= scientific.size() ? fixed : scientific;
    }
    int written = snprintf_c_locale(buffer.data(), buffer.size(), true, 16, number);
    return std::string(buffer.data(), buffer.data() + written);
}

#endif

double parse_double(std::string_view text, bool &ok)
{
#if MEIOS_HAS_FLOAT_CHARCONV
    const char *first = text.data();
    const char *last = text.data() + text.size();
    double number = 0.0;
    std::from_chars_result result = std::from_chars(first, last, number);
    ok = result.ec == std::errc{} && result.ptr == last;
    return ok ? number : 0.0;
#else
    std::string buffer(text);
    char *stop = nullptr;
    errno = 0;
    double number = strtod_c_locale(buffer.c_str(), &stop);
    ok = errno == 0 && stop == buffer.c_str() + buffer.size() && !buffer.empty();
    return ok ? number : 0.0;
#endif
}

std::int64_t parse_int(std::string_view text, bool &ok)
{
    const char *first = text.data();
    const char *last = text.data() + text.size();
    std::int64_t number = 0;
    std::from_chars_result result = std::from_chars(first, last, number);
    ok = result.ec == std::errc{} && result.ptr == last;
    return ok ? number : 0;
}

std::string print_double(double number)
{
#if MEIOS_HAS_FLOAT_CHARCONV
    std::array<char, 64> buffer{};
    std::to_chars_result result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
    std::string text(buffer.data(), result.ptr);
#else
    std::string text = print_double_shortest(number);
#endif
    // Python str(1.0)=="1.0" but to_chars(1.0)=="1"; append ".0" to an integral
    // form so the printed value matches xacro's output.
    if(text.find_first_of(".eEnN") == std::string::npos)
        text += ".0";
    return text;
}

}

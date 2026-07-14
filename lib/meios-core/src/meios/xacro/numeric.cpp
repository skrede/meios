#include "meios/xacro/detail/numeric.h"

#include <array>
#include <cstdio>
#include <cerrno>
#include <cstddef>
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

#endif

double parse_double(std::string_view text, bool &ok)
{
    const char *first = text.data();
    const char *last = text.data() + text.size();
#if MEIOS_HAS_FLOAT_CHARCONV
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

long long parse_int(std::string_view text, bool &ok)
{
    const char *first = text.data();
    const char *last = text.data() + text.size();
    long long number = 0;
    std::from_chars_result result = std::from_chars(first, last, number);
    ok = result.ec == std::errc{} && result.ptr == last;
    return ok ? number : 0;
}

std::string print_double(double number)
{
    std::array<char, 64> buffer{};
#if MEIOS_HAS_FLOAT_CHARCONV
    std::to_chars_result result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
    std::string text(buffer.data(), result.ptr);
#else
    int written = std::snprintf(buffer.data(), buffer.size(), "%.17g", number);
    std::string text(buffer.data(), buffer.data() + written);
#endif
    // Python str(1.0)=="1.0" but to_chars(1.0)=="1"; append ".0" to an integral
    // form so the printed value matches xacro's output.
    if(text.find_first_of(".eEnN") == std::string::npos)
        text += ".0";
    return text;
}

}

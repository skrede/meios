#include "numeric_detail.h"

#include "meios/xacro/detail/numeric.h"

#include <cmath>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <charconv>
#include <string_view>
#include <system_error>

namespace meios::detail
{

#if !MEIOS_HAS_FLOAT_CHARCONV

// Fallback for toolchains (historically libc++) lacking float from_chars/to_chars.
// strtod/snprintf run under an explicit "C" locale so a comma-decimal global
// locale cannot corrupt URDF "."-decimals; charconv is already locale-free.
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

namespace
{

// from_chars reads strtod's "C"-locale subject sequence less the leading white space, the plus
// sign and the hexadecimal form ([charconv.from.chars]).
bool strtod_only_spelling(std::string_view text)
{
    const std::string_view body = text.substr(text.starts_with('-') ? 1 : 0);
    if(body.empty() || std::string_view("0123456789.iInN").find(body.front()) == std::string_view::npos)
        return true;
    return body.starts_with("0x") || body.starts_with("0X");
}

// from_chars refuses a decimal that rounds to an infinity, or a nonzero one that rounds to zero, and
// reads a subnormal; strtod's ERANGE flags the subnormal too, so the range is judged by the value.
bool outside_double_range(std::string_view text, double number)
{
    if(std::isinf(number))
        return text.find_first_of("iI") == std::string_view::npos;
    const std::string_view mantissa = text.substr(0, text.find_first_of("eE"));
    return number == 0.0 && mantissa.find_first_of("123456789") != std::string_view::npos;
}

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
    const std::string buffer(text);
    char *stop = nullptr;
    const double number = strtod_c_locale(buffer.c_str(), &stop);
    ok = stop == buffer.c_str() + buffer.size() && !strtod_only_spelling(text) && !outside_double_range(text, number);
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
    std::string text = print_double_shortest(number);
    // Python str(1.0)=="1.0" but to_chars(1.0)=="1"; append ".0" to an integral
    // form so the printed value matches xacro's output.
    if(text.find_first_of(".eEnN") == std::string::npos)
        text += ".0";
    return text;
}

}

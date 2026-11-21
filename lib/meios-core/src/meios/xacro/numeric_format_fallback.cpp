#include "numeric_detail.h"

#include "meios/xacro/detail/numeric.h"

#include <array>
#include <cstdio>
#include <string>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>

namespace meios::detail
{

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

struct decimal_digits
{
    bool         negative;
    std::string  digits;
    std::int32_t exponent;
};

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

bool reads_back(const std::string &text, double number)
{
    char *stop = nullptr;
    return strtod_c_locale(text.c_str(), &stop) == number;
}

decimal_digits digits_of(const std::string &scientific)
{
    const std::size_t e_pos = scientific.find('e');
    decimal_digits read{ scientific.front() == '-', {}, std::atoi(scientific.c_str() + e_pos + 1) };
    for(std::size_t at = 0; at < e_pos; ++at)
        if(scientific[at] >= '0' && scientific[at] <= '9')
            read.digits += scientific[at];
    return read;
}

decimal_digits stepped_up(decimal_digits value)
{
    std::string &digits = value.digits;
    if(digits.find_first_not_of('9') == std::string::npos)
        return { value.negative, '1' + std::string(digits.size() - 1, '0'), value.exponent + 1 };
    const std::size_t at = digits.find_last_not_of('9');
    ++digits[at];
    digits.replace(at + 1, std::string::npos, digits.size() - at - 1, '0');
    return value;
}

decimal_digits stepped_down(decimal_digits value)
{
    std::string &digits = value.digits;
    if(digits.front() == '1' && digits.find_first_not_of('0', 1) == std::string::npos)
        return { value.negative, std::string(digits.size(), '9'), value.exponent - 1 };
    const std::size_t at = digits.find_last_not_of('0');
    --digits[at];
    digits.replace(at + 1, std::string::npos, digits.size() - at - 1, '9');
    return value;
}

std::string scientific_text(const decimal_digits &value)
{
    std::array<char, 8> exponent{};
    std::snprintf(exponent.data(), exponent.size(), "e%+03d", static_cast<int>(value.exponent));
    std::string text = value.negative ? "-" : "";
    text += value.digits.front();
    if(value.digits.size() > 1)
        text += '.' + value.digits.substr(1);
    return text + exponent.data();
}

std::string fixed_text(const decimal_digits &value)
{
    const bool below_one = value.exponent < 0;
    std::string text = std::string(below_one ? static_cast<std::size_t>(-value.exponent) : 0, '0') + value.digits;
    const std::size_t point = below_one ? 1 : static_cast<std::size_t>(value.exponent) + 1;
    if(text.size() < point)
        text.append(point - text.size(), '0');
    else if(text.size() > point)
        text.insert(point, 1, '.');
    return value.negative ? '-' + text : text;
}

std::string spelled(const decimal_digits &value)
{
    if(choose_notation(value.exponent) == notation::fixed)
        return fixed_text(value);
    const std::string scientific = scientific_text(value);
    return trim_mantissa(scientific, scientific.find('e'));
}

std::string rounded_spelling(const std::string &scientific, std::int32_t significant, double number)
{
    const std::size_t e_pos = scientific.find('e');
    const std::int32_t exponent = std::atoi(scientific.c_str() + e_pos + 1);
    if(choose_notation(exponent) == notation::scientific)
        return trim_mantissa(scientific, e_pos);
    std::array<char, 64> buffer{};
    const int written = snprintf_c_locale(buffer.data(), buffer.size(), false, fixed_fraction(significant, exponent), number);
    return std::string(buffer.data(), buffer.data() + written);
}

// Below a power of two the rounding interval is half the one above, so the correctly rounded
// spelling can fall outside it while its neighbor on the value's other side falls inside.
std::optional<std::string> neighbor_reading_back(const std::string &scientific, double number)
{
    const decimal_digits nearest = digits_of(scientific);
    for(const decimal_digits &neighbor : { stepped_up(nearest), stepped_down(nearest) })
        if(reads_back(scientific_text(neighbor), number))
            return spelled(neighbor);
    return std::nullopt;
}

}

// Shortest round-trip double->string for toolchains (libc++) lacking float to_chars. At each
// count of significant digits, fewest first, it tries the correctly rounded scientific form and
// then that form's two neighbors, and spells the first that parses bit-exactly back through
// strtod_c_locale in the chosen notation. "%.16e" always round-trips a finite double, so the
// loop ends there. Only finite values reach here; the entry point holds the guard.
std::string print_finite_shortest(double number)
{
    std::array<char, 64> buffer{};
    for(std::int32_t significant = 1; significant <= 17; ++significant)
    {
        const int written = snprintf_c_locale(buffer.data(), buffer.size(), true, significant - 1, number);
        const std::string scientific(buffer.data(), buffer.data() + written);
        if(reads_back(scientific, number))
            return rounded_spelling(scientific, significant, number);
        if(std::optional<std::string> neighbor = neighbor_reading_back(scientific, number))
            return *neighbor;
    }
    const int written = snprintf_c_locale(buffer.data(), buffer.size(), true, 16, number);
    return std::string(buffer.data(), buffer.data() + written);
}

#endif

}

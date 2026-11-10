#include "python_space.h"
#include "python_number.h"

#include "meios/detail/utf8.h"

#include "meios/xacro/detail/numeric.h"

#include <limits>
#include <string>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

constexpr std::size_t none = std::string_view::npos;

// The zero of every run of ten decimal digits Python 3.14 (Unicode 16.0) reads in int() and
// float(); each run holds zero to nine in order.
constexpr char32_t digit_zeros[] = {0x30,    0x660,   0x6F0,   0x7C0,   0x966,   0x9E6,   0xA66,   0xAE6,   0xB66,   0xBE6,   0xC66,   0xCE6,   0xD66,   0xDE6,   0xE50,   0xED0,
                                    0xF20,   0x1040,  0x1090,  0x17E0,  0x1810,  0x1946,  0x19D0,  0x1A80,  0x1A90,  0x1B50,  0x1BB0,  0x1C40,  0x1C50,  0xA620,  0xA8D0,  0xA900,
                                    0xA9D0,  0xA9F0,  0xAA50,  0xABF0,  0xFF10,  0x104A0, 0x10D30, 0x10D40, 0x11066, 0x110F0, 0x11136, 0x111D0, 0x112F0, 0x11450, 0x114D0, 0x11650,
                                    0x116C0, 0x116D0, 0x116DA, 0x11730, 0x118E0, 0x11950, 0x11BF0, 0x11C50, 0x11D50, 0x11DA0, 0x11F50, 0x16130, 0x16A60, 0x16AC0, 0x16B50, 0x16D70,
                                    0x1CCF0, 0x1D7CE, 0x1D7D8, 0x1D7E2, 0x1D7EC, 0x1D7F6, 0x1E140, 0x1E2F0, 0x1E4F0, 0x1E5F1, 0x1E950, 0x1FBF0};

// Appends the digit at the offset in its ASCII spelling and answers the offset after it.
std::size_t take_digit(std::string_view text, std::size_t at, std::string &out)
{
    if(at >= text.size())
        return none;
    const utf8_point one = decode_utf8(text, at);
    for(char32_t zero : digit_zeros)
        if(one.value >= zero && one.value < zero + 10)
        {
            out += static_cast<char>('0' + (one.value - zero));
            return at + one.length;
        }
    return none;
}

std::size_t digit_run(std::string_view text, std::size_t at, std::string &out)
{
    at = take_digit(text, at, out);
    while(at != none && at < text.size())
    {
        const std::size_t next = take_digit(text, text[at] == '_' ? at + 1 : at, out);
        if(next == none)
            break;
        at = next;
    }
    return at;
}

std::size_t sign(std::string_view text, std::size_t at, std::string &out)
{
    if(at >= text.size() || (text[at] != '+' && text[at] != '-'))
        return at;
    if(text[at] == '-')
        out += '-';
    return at + 1;
}

// str.isspace() holds the separators U+001C to U+001F, which int() and float() do not strip, so a
// text holding one never reads as a number.
std::string_view number_text(std::string_view text)
{
    const bool separator = std::ranges::any_of(text, [](char c) { return c >= '\x1c' && c <= '\x1f'; });
    return separator ? std::string_view{} : python_stripped(text);
}

std::optional<double> named_real(std::string_view text)
{
    std::string lower;
    for(char c : text)
        lower += c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    if(lower == "inf" || lower == "infinity")
        return std::numeric_limits<double>::infinity();
    if(lower == "nan")
        return std::numeric_limits<double>::quiet_NaN();
    return std::nullopt;
}

// Digits with an optional fraction, or a fraction alone, then an optional exponent, in ASCII.
std::size_t decimal(std::string_view text, std::size_t at, std::string &out)
{
    const std::size_t whole = digit_run(text, at, out);
    std::size_t next        = whole == none ? at : whole;
    std::size_t fraction    = none;
    if(next < text.size() && text[next] == '.')
    {
        fraction = digit_run(text, next + 1, out += '.');
        next     = fraction == none ? next + 1 : fraction;
    }
    if(whole == none && fraction == none)
        return none;
    if(next < text.size() && (text[next] == 'e' || text[next] == 'E'))
        next = digit_run(text, sign(text, next + 1, out += 'e'), out);
    return next;
}

// A well-formed spelling from_chars refuses rounds to an infinity or to zero; the decimal exponent
// of its leading significant digit decides which.
double out_of_range(std::string_view spelled)
{
    const bool negative             = spelled.starts_with('-');
    const std::size_t mark          = std::min(spelled.find('e'), spelled.size());
    const std::string_view mantissa = spelled.substr(negative ? 1 : 0, mark - (negative ? 1 : 0));
    const std::string_view exponent = mark < spelled.size() ? spelled.substr(mark + 1) : "0";
    const std::size_t point         = std::min(mantissa.find('.'), mantissa.size());
    const std::size_t lead          = mantissa.find_first_not_of("0.");
    const std::int64_t leading      = static_cast<std::int64_t>(point) - static_cast<std::int64_t>(lead) - (lead < point ? 1 : 0);
    bool ok                         = false;
    const std::int64_t power        = parse_int(exponent, ok);
    const bool large                = lead != none && (ok ? power > -leading : !exponent.starts_with('-'));
    const double magnitude          = large ? std::numeric_limits<double>::infinity() : 0.0;
    return negative ? -magnitude : magnitude;
}

}

std::optional<std::string> python_int(std::string_view text)
{
    text = number_text(text);
    std::string digits;
    if(digit_run(text, sign(text, 0, digits), digits) != text.size())
        return std::nullopt;
    std::string_view body(digits);
    const bool negative = body.starts_with('-');
    body.remove_prefix(negative ? 1 : 0);
    body.remove_prefix(std::min(body.find_first_not_of('0'), body.size() - 1));
    return negative && body != "0" ? '-' + std::string(body) : std::string(body);
}

std::optional<double> python_real(std::string_view text)
{
    text = number_text(text);
    std::string spelled;
    const std::size_t start           = sign(text, 0, spelled);
    const std::optional<double> named = named_real(text.substr(start));
    if(named)
        return spelled.empty() ? *named : -*named;
    if(decimal(text, start, spelled) != text.size())
        return std::nullopt;
    bool ok             = false;
    const double number = parse_double(spelled, ok);
    return ok ? number : out_of_range(spelled);
}

}

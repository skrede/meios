#include "python_space.h"
#include "argument_conversion.h"

#include "meios/detail/utf8.h"

#include "meios/xacro/value.h"
#include "meios/xacro/detail/numeric.h"

#include <cctype>
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
        return std::string_view::npos;
    const utf8_point one = decode_utf8(text, at);
    for(char32_t zero : digit_zeros)
        if(one.value >= zero && one.value < zero + 10)
        {
            out += static_cast<char>('0' + (one.value - zero));
            return at + one.length;
        }
    return std::string_view::npos;
}

// Python's int() and float() read single underscores between digits, which from_chars does not.
std::size_t digit_run(std::string_view text, std::size_t at, std::string &out)
{
    at = take_digit(text, at, out);
    while(at != std::string_view::npos && at < text.size())
    {
        const std::size_t next = take_digit(text, text[at] == '_' ? at + 1 : at, out);
        if(next == std::string_view::npos)
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

std::optional<value> integer_spelling(std::string_view text)
{
    std::string digits;
    if(digit_run(text, sign(text, 0, digits), digits) != text.size())
        return value{std::string(text)};
    bool ok                   = false;
    const std::int64_t number = parse_int(digits, ok);
    return ok ? std::optional<value>(value{number}) : std::nullopt;
}

std::optional<value> real_spelling(std::string_view text)
{
    std::string digits;
    const std::size_t start = sign(text, 0, digits);
    const std::size_t whole = digit_run(text, start, digits);
    std::size_t at          = whole == std::string_view::npos ? start : whole;
    if(at >= text.size() || text[at] != '.')
        return value{std::string(text)};
    digits += '.';
    const std::size_t fraction = digit_run(text, at + 1, digits);
    at                         = fraction == std::string_view::npos ? at + 1 : fraction;
    if(at < text.size() && (text[at] == 'e' || text[at] == 'E'))
        at = digit_run(text, sign(text, at + 1, digits += 'e'), digits);
    if(at != text.size() || (whole == std::string_view::npos && fraction == std::string_view::npos))
        return value{std::string(text)};
    bool ok             = false;
    const double number = parse_double(digits, ok);
    return ok ? value::make_real(number) : std::nullopt;
}

std::string lowered(std::string_view text)
{
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

}

std::optional<value> converted_argument(std::string_view text)
{
    const std::string_view number   = python_stripped(text);
    const std::optional<value> read = text.find('.') == std::string_view::npos ? integer_spelling(number) : real_spelling(number);
    if(!read || read->kind() != value_kind::string)
        return read;
    const std::string spelling = lowered(text);
    if(spelling == "true" || spelling == "false")
        return value{spelling == "true"};
    return value{std::string(text)};
}

}

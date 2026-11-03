#include "nfkc.h"
#include "unicode_data.h"
#include "identifier_text.h"
#include "reference_names.h"

#include "meios/detail/utf8.h"

#include <span>
#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <charconv>
#include <iterator>
#include <optional>
#include <algorithm>
#include <functional>
#include <string_view>

namespace meios::detail
{

namespace
{

constexpr std::string_view python_constants[] = {"None", "True", "False"};

bool ascii_start(char32_t point)
{
    return (point >= 'a' && point <= 'z') || (point >= 'A' && point <= 'Z') || point == '_';
}

bool ascii_continue(char32_t point)
{
    return ascii_start(point) || (point >= '0' && point <= '9');
}

// NFKC leaves ASCII unchanged, so an ASCII identifier answers itself and reads no table.
bool ascii_identifier(std::string_view spelling)
{
    const auto continuing = [](char c) { return ascii_continue(static_cast<std::uint8_t>(c)); };
    return !spelling.empty() && ascii_start(static_cast<std::uint8_t>(spelling.front())) && std::ranges::all_of(spelling.substr(1), continuing);
}

bool within(std::span<const code_point_range> table, char32_t point)
{
    const auto found = std::ranges::lower_bound(table, point, std::ranges::less{}, &code_point_range::last);
    return found != table.end() && found->first <= point;
}

bool starts(char32_t point)
{
    return point < 0x80 ? ascii_start(point) : within(xid_start_ranges(), point);
}

bool continues(char32_t point)
{
    return point < 0x80 ? ascii_continue(point) : within(xid_start_ranges(), point) || within(xid_continue_ranges(), point);
}

std::optional<std::u32string> identifier_points(std::string_view spelling)
{
    std::u32string points;
    for(std::size_t at = 0; at < spelling.size();)
    {
        const utf8_point one = decode_utf8(spelling, at);
        if(one.value == malformed_utf8 || !(points.empty() ? starts(one.value) : continues(one.value)))
            return std::nullopt;
        points += one.value;
        at += one.length;
    }
    return points.empty() ? std::nullopt : std::optional<std::u32string>(std::move(points));
}

std::string encoded(std::u32string_view points)
{
    std::string out;
    for(char32_t point : points)
        append_utf8(out, point);
    return out;
}

std::string hex_digits(std::uint32_t value, std::size_t width)
{
    char digits[8];
    const std::to_chars_result written = std::to_chars(std::begin(digits), std::end(digits), value, 16);
    std::string out(std::begin(digits), written.ptr);
    std::ranges::transform(out, out.begin(), [](char c) { return c >= 'a' ? static_cast<char>(c - 'a' + 'A') : c; });
    return std::string(width > out.size() ? width - out.size() : 0, '0') + out;
}

bool printable(char32_t point)
{
    return point < 0x80 ? point >= 0x20 && point < 0x7F : within(printable_ranges(), point);
}

// Parser/lexer/lexer.c verify_identifier: the character is quoted only when it is printable.
std::string character_message(char32_t point)
{
    const std::string code = "U+" + hex_digits(point, 4);
    if(!printable(point))
        return "invalid non-printable character " + code;
    std::string message = "invalid character '";
    append_utf8(message, point);
    return message + "' (" + code + ")";
}

}

std::optional<std::string> python_name(std::string_view spelling)
{
    if(ascii_identifier(spelling))
        return std::string(spelling);
    const std::optional<std::u32string> points = identifier_points(spelling);
    if(!points)
        return std::nullopt;
    return encoded(nfkc(*points));
}

bool valid_python_name(std::string_view name)
{
    const std::optional<std::string> form = python_name(name);
    return form && *form == name && !listed(python_keywords, name);
}

bool opens_identifier(char byte)
{
    return static_cast<std::uint8_t>(byte) >= 0x80 || ascii_start(static_cast<std::uint8_t>(byte));
}

std::size_t identifier_run(std::string_view source, std::size_t at)
{
    while(at < source.size() && (opens_identifier(source[at]) || (source[at] >= '0' && source[at] <= '9')))
        ++at;
    return at;
}

std::optional<std::string> read_spelling(std::string_view run)
{
    std::optional<std::string> form = python_name(run);
    if(form && *form != run && listed(python_constants, *form))
        return std::nullopt;
    return form;
}

std::optional<std::string> identifier_fault(std::string_view run)
{
    if(run.empty() || !opens_identifier(run.front()) || read_spelling(run))
        return std::nullopt;
    for(std::size_t at = 0; at < run.size();)
    {
        const utf8_point one = decode_utf8(run, at);
        if(one.value == malformed_utf8)
            return "invalid UTF-8 in expression at byte 0x" + hex_digits(static_cast<std::uint8_t>(run[at]), 2);
        if(!(at == 0 ? starts(one.value) : continues(one.value)))
            return character_message(one.value);
        at += one.length;
    }
    return "identifier field can't represent '" + python_name(run).value_or(std::string()) + "' constant";
}

}

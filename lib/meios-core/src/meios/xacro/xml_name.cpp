#include "xml_name.h"

#include "meios/detail/utf8.h"

#include <array>
#include <string>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

struct span
{
    char32_t first;
    char32_t last;
};

// XML 1.0 (fifth edition), section 2.3, productions [4] NameStartChar, [4a] NameChar and [5] Name.
constexpr std::array<span, 15> start_spans{{
        {':', ':'}, {'A', 'Z'}, {'_', '_'}, {'a', 'z'}, {0xC0, 0xD6}, {0xD8, 0xF6}, {0xF8, 0x2FF},
        {0x370, 0x37D}, {0x37F, 0x1FFF}, {0x200C, 0x200D}, {0x2070, 0x218F}, {0x2C00, 0x2FEF},
        {0x3001, 0xD7FF}, {0xF900, 0xFDCF}, {0xFDF0, 0xFFFD},
}};

constexpr std::array<span, 6> more_spans{{
        {'-', '-'}, {'.', '.'}, {'0', '9'}, {0xB7, 0xB7}, {0x300, 0x36F}, {0x203F, 0x2040},
}};

template <std::size_t N>
bool within(const std::array<span, N> &spans, char32_t point)
{
    return std::ranges::any_of(spans, [point](const span &one) { return point >= one.first && point <= one.last; });
}

bool name_start(char32_t point)
{
    return within(start_spans, point) || (point >= 0x10000 && point <= 0xEFFFF);
}

std::string visible(std::uint8_t byte)
{
    if(byte == '\t')
        return "\\t";
    if(byte == '\r')
        return "\\r";
    if(byte == '\n')
        return "\\n";
    constexpr std::string_view hex = "0123456789abcdef";
    return {'\\', 'x', hex[byte >> 4], hex[byte & 0xF]};
}

}

bool is_xml_name(std::string_view text)
{
    std::size_t at = 0;
    while(at < text.size())
    {
        const utf8_point point = decode_utf8(text, at);
        if(point.value == malformed_utf8)
            return false;
        if(!name_start(point.value) && (at == 0 || !within(more_spans, point.value)))
            return false;
        at += point.length;
    }
    return !text.empty();
}

std::string_view trim_xml_space(std::string_view text)
{
    constexpr std::string_view space = " \t\r\n";
    const std::size_t first = text.find_first_not_of(space);
    if(first == std::string_view::npos)
        return {};
    return text.substr(first, text.find_last_not_of(space) - first + 1);
}

std::string escaped_controls(std::string_view text)
{
    std::string out;
    for(const char one : text)
    {
        const auto byte = static_cast<std::uint8_t>(one);
        if(byte < 0x20 || byte == 0x7F)
            out += visible(byte);
        else
            out += one;
    }
    return out;
}

}

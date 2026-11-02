#ifndef HPP_GUARD_MEIOS_DETAIL_UTF8_H
#define HPP_GUARD_MEIOS_DETAIL_UTF8_H

#include <string>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace meios::detail
{

struct utf8_point
{
    char32_t value;
    std::size_t length;
};

inline constexpr char32_t malformed_utf8 = 0xFFFFFFFF;

inline std::size_t utf8_sequence_length(std::uint8_t lead)
{
    return lead < 0x80 ? 1 : lead < 0xC2 ? 0 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : lead < 0xF5 ? 4 : 0;
}

// RFC 3629 section 4: a well-formed sequence is the shortest form of its value, and no value is a
// surrogate or above U+10FFFF.
inline bool utf8_scalar(char32_t value, std::size_t length)
{
    constexpr char32_t least[] = {0, 0, 0x80, 0x800, 0x10000};
    return value >= least[length] && value <= 0x10FFFF && (value < 0xD800 || value > 0xDFFF);
}

inline utf8_point decode_utf8(std::string_view text, std::size_t at)
{
    const auto lead          = static_cast<std::uint8_t>(text[at]);
    const std::size_t length = utf8_sequence_length(lead);
    if(length == 0 || at + length > text.size())
        return utf8_point{malformed_utf8, 1};
    char32_t value = length == 1 ? lead : static_cast<char32_t>(lead & (0x7F >> length));
    for(std::size_t k = 1; k < length; ++k)
    {
        const auto next = static_cast<std::uint8_t>(text[at + k]);
        if((next & 0xC0) != 0x80)
            return utf8_point{malformed_utf8, 1};
        value = (value << 6) | static_cast<char32_t>(next & 0x3F);
    }
    return utf8_scalar(value, length) ? utf8_point{value, length} : utf8_point{malformed_utf8, 1};
}

inline void append_utf8(std::string &out, char32_t point)
{
    constexpr std::uint8_t leads[] = {0x00, 0x00, 0xC0, 0xE0, 0xF0};
    const std::size_t length       = point < 0x80 ? 1 : point < 0x800 ? 2 : point < 0x10000 ? 3 : 4;
    out += static_cast<char>(leads[length] | (point >> (6 * (length - 1))));
    for(std::size_t k = length - 1; k > 0; --k)
        out += static_cast<char>(0x80 | ((point >> (6 * (k - 1))) & 0x3F));
}

}

#endif

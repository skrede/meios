#ifndef HPP_GUARD_MEIOS_MODEL_DETAIL_ESCAPED_CONTROLS_H
#define HPP_GUARD_MEIOS_MODEL_DETAIL_ESCAPED_CONTROLS_H

#include "meios/detail/utf8.h"

#include <string>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace meios::detail
{

inline void append_hex(std::string &out, char32_t value, std::size_t digits)
{
    constexpr std::string_view hex = "0123456789abcdef";
    for(std::size_t k = digits; k > 0; --k)
        out += hex[(value >> (4 * (k - 1))) & 0xF];
}

inline bool escaped_byte(char32_t point)
{
    return point < 0x20 || point == 0x7F || point == '\\';
}

inline bool escaped_point(char32_t point)
{
    return (point >= 0x80 && point <= 0x9F) || point == 0x2028 || point == 0x2029;
}

inline void append_visible_byte(std::string &out, char32_t byte)
{
    if(byte == '\t')
        out += "\\t";
    else if(byte == '\r')
        out += "\\r";
    else if(byte == '\n')
        out += "\\n";
    else if(byte == '\\')
        out += "\\\\";
    else
    {
        out += "\\x";
        append_hex(out, byte, 2);
    }
}

inline void append_visible_point(std::string &out, char32_t point)
{
    out += "\\u";
    append_hex(out, point, 4);
}

// A tab, carriage return or line feed is written \t, \r or \n, a backslash \\, any other C0 control, DEL or stray byte
// in 0x80..0x9F as \x and two hex digits, and U+0080..U+009F, U+2028 or U+2029 as \u and four. Every backslash the
// output holds begins an escape, so a record stays on one line and reads back to the text raised.
inline std::string escaped_controls(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    std::size_t at = 0;
    while(at < text.size())
    {
        const utf8_point point = decode_utf8(text, at);
        const auto byte        = static_cast<std::uint8_t>(text[at]);
        // A stray byte above 0x9F is copied: a Windows path is written in the ANSI code page, where such
        // bytes are ordinary characters.
        if(point.value == malformed_utf8 && byte > 0x9F)
            out += text[at];
        else if(point.value == malformed_utf8 || escaped_byte(point.value))
            append_visible_byte(out, byte);
        else if(escaped_point(point.value))
            append_visible_point(out, point.value);
        else
            out += text.substr(at, point.length);
        at += point.length;
    }
    return out;
}

}

#endif

#ifndef HPP_GUARD_MEIOS_TESTS_MARKER_SPELLING_H
#define HPP_GUARD_MEIOS_TESTS_MARKER_SPELLING_H

#include <meios/xacro/container_marker.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace marker
{

// pugixml writes a control character as a two-digit decimal character reference and never as its
// own byte (text_output_escaped), so a marker surviving a strip reaches a serialized document
// spelled "&#01;"; a raw-byte search alone asks for a spelling the serializer cannot produce.
inline std::string as_written(char one)
{
    const int code = static_cast<int>(one);
    return std::string{ "&#" } + static_cast<char>('0' + code / 10)
        + static_cast<char>('0' + code % 10) + ';';
}

// The hexadecimal spelling an author writes, which the XML parser resolves to the byte itself
// before any of this code sees the text.
inline std::string as_referenced(char one)
{
    constexpr std::string_view digits = "0123456789abcdef";
    const unsigned code = static_cast<unsigned char>(one);
    return std::string{ "&#x" } + digits[code >> 4] + digits[code & 0xFu] + ';';
}

// Every value reaches a document through the output strip, so this reports on that strip alone
// and only where the case emits a marker-bearing value whole; a case rendering a scalar derived
// from one cannot make it fail whatever any strip does.
inline void absent_from(const std::string &document)
{
    for(char one : meios::detail::container_markers)
    {
        INFO("marker byte " << static_cast<int>(one));
        CHECK(document.find(one) == std::string::npos);
        CHECK(document.find(as_written(one)) == std::string::npos);
    }
}

inline bool forged_in(const std::string &authored)
{
    for(char one : meios::detail::container_markers)
    {
        if(authored.find(one) != std::string::npos
           || authored.find(as_referenced(one)) != std::string::npos)
            return true;
    }
    return false;
}

}

#endif

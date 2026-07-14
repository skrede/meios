#ifndef HPP_GUARD_MEIOS_CLI_JSON_ESCAPE_H
#define HPP_GUARD_MEIOS_CLI_JSON_ESCAPE_H

#include <string>
#include <cstdio>

namespace meios::cli
{

// Escapes a string for a JSON double-quoted scalar: the two structural characters
// plus every C0 control byte (RFC 8259 §7 forbids raw control bytes in strings).
inline std::string json_escape(const std::string &text)
{
    std::string out;
    for(const char raw : text)
    {
        const unsigned char byte = static_cast<unsigned char>(raw);
        if(raw == '"' || raw == '\\')
            out += { '\\', raw };
        else if(byte < 0x20)
        {
            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "\\u%04x", byte);
            out += buffer;
        }
        else
            out += raw;
    }
    return out;
}

}

#endif

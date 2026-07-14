#include "label_escape.h"

#include <string>
#include <string_view>

namespace meios::cli
{

std::string escape_dot(std::string_view name)
{
    std::string out;
    for(const char raw : name)
    {
        const unsigned char byte = static_cast<unsigned char>(raw);
        if(raw == '"' || raw == '\\')
            out += { '\\', raw };
        else if(raw == '\n')
            out += "\\n";
        else if(raw == '\r')
            out += "\\r";
        else if(raw == '\t')
            out += "\\t";
        else if(byte >= 0x20 && byte != 0x7f)
            out += raw;
    }
    return out;
}

std::string escape_ascii(std::string_view name)
{
    std::string out;
    for(const char raw : name)
    {
        const unsigned char byte = static_cast<unsigned char>(raw);
        out += (byte < 0x20 || byte == 0x7f) ? '?' : raw;
    }
    return out;
}

}

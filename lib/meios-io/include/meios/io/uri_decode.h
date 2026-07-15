#ifndef HPP_GUARD_MEIOS_IO_URI_DECODE_H
#define HPP_GUARD_MEIOS_IO_URI_DECODE_H

#include <string>
#include <cstddef>
#include <string_view>

namespace meios::detail
{

inline int hex_value(char c)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline std::string percent_decode(std::string_view uri)
{
    std::string out;
    out.reserve(uri.size());
    for(std::size_t i = 0; i < uri.size(); ++i)
    {
        const int hi = i + 2 < uri.size() ? hex_value(uri[i + 1]) : -1;
        const int lo = i + 2 < uri.size() ? hex_value(uri[i + 2]) : -1;
        if(uri[i] == '%' && hi >= 0 && lo >= 0)
        {
            out.push_back(static_cast<char>(hi * 16 + lo));
            i += 2;
        }
        else
            out.push_back(uri[i]);
    }
    return out;
}

}

#endif

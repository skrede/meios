#include "python_space.h"

#include "meios/detail/utf8.h"

#include <cstddef>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

// What Python 3.14's str.isspace() holds, which str.strip(), int() and float() strip from either end.
constexpr char32_t spaces[] = {0x9,    0xA,    0xB,    0xC,    0xD,    0x1C,   0x1D,   0x1E,   0x1F,   0x20,   0x85,   0xA0,   0x1680, 0x2000, 0x2001,
                               0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A, 0x2028, 0x2029, 0x202F, 0x205F, 0x3000};

bool space(char32_t point)
{
    return std::ranges::find(spaces, point) != std::ranges::end(spaces);
}

}

std::string_view python_stripped(std::string_view text)
{
    std::size_t begin = std::string_view::npos;
    std::size_t end   = 0;
    for(std::size_t at = 0; at < text.size();)
    {
        const utf8_point one = decode_utf8(text, at);
        if(!space(one.value))
        {
            begin = std::min(begin, at);
            end   = at + one.length;
        }
        at += one.length;
    }
    return begin == std::string_view::npos ? std::string_view{} : text.substr(begin, end - begin);
}

}

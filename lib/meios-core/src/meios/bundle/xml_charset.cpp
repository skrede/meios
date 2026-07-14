#include "writer_detail.h"

#include <cstddef>
#include <string_view>

namespace meios::detail
{

namespace
{

int sequence_length(unsigned char lead)
{
    if(lead < 0x80)
        return 1;
    if((lead >> 5) == 0x6)
        return 2;
    if((lead >> 4) == 0xE)
        return 3;
    if((lead >> 3) == 0x1E)
        return 4;
    return 0;
}

bool decode(std::string_view text, std::size_t &i, char32_t &cp)
{
    const unsigned char lead = static_cast<unsigned char>(text[i]);
    const int length = sequence_length(lead);
    if(length == 0 || i + static_cast<std::size_t>(length) > text.size())
        return false;
    static const char32_t lead_mask[5] = { 0, 0x7F, 0x1F, 0x0F, 0x07 };
    cp = lead & lead_mask[length];
    for(int k = 1; k < length; ++k)
    {
        const unsigned char byte = static_cast<unsigned char>(text[i + static_cast<std::size_t>(k)]);
        if((byte >> 6) != 0x2)
            return false;
        cp = (cp << 6) | (byte & 0x3F);
    }
    i += static_cast<std::size_t>(length);
    return true;
}

// W3C XML 1.0 §2.2 Char production: #x9 | #xA | #xD | [#x20-#xD7FF] |
// [#xE000-#xFFFD] | [#x10000-#x10FFFF]. Raw C0 controls outside tab/LF/CR are
// the un-encodable class pugixml would otherwise emit as illegal &#n; references.
bool codepoint_ok(char32_t cp)
{
    if(cp == 0x9 || cp == 0xA || cp == 0xD)
        return true;
    if(cp >= 0x20 && cp <= 0xD7FF)
        return true;
    if(cp >= 0xE000 && cp <= 0xFFFD)
        return true;
    return cp >= 0x10000 && cp <= 0x10FFFF;
}

}

bool xml10_char_ok(std::string_view text)
{
    std::size_t i = 0;
    while(i < text.size())
    {
        char32_t cp = 0;
        if(!decode(text, i, cp) || !codepoint_ok(cp))
            return false;
    }
    return true;
}

}

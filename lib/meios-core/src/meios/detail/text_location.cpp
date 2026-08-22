#include "text_location.h"

#include <vector>
#include <string>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

bool is_xml_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

struct value_span
{
    std::size_t start;
    std::size_t stop;
};

// Replays the one already-validated start tag pugixml parsed, recovering the raw byte
// range (between the quotes) of the attribute value at DOM position `target`. The
// match is positional, never by name: pugixml permits duplicate and namespace-prefixed
// names, and position is exact under both. The scan is bounded only by the matching
// quote -- the other quote character and even '>' may legally appear inside a value.
std::optional<value_span> locate_attr_value(std::string_view t, std::size_t pos, std::size_t target)
{
    const std::size_t n = t.size();
    while(pos < n && !is_xml_space(t[pos]) && t[pos] != '/' && t[pos] != '>')
        ++pos;
    for(std::size_t index = 0; pos < n; ++index)
    {
        while(pos < n && is_xml_space(t[pos]))
            ++pos;
        if(pos >= n || t[pos] == '/' || t[pos] == '>')
            return std::nullopt;
        while(pos < n && t[pos] != '=' && !is_xml_space(t[pos]))
            ++pos;
        while(pos < n && is_xml_space(t[pos]))
            ++pos;
        if(pos >= n || t[pos] != '=')
            return std::nullopt;
        ++pos;
        while(pos < n && is_xml_space(t[pos]))
            ++pos;
        if(pos >= n || (t[pos] != '"' && t[pos] != '\''))
            return std::nullopt;
        const char quote = t[pos];
        ++pos;
        const std::size_t start = pos;
        while(pos < n && t[pos] != quote)
            ++pos;
        if(pos >= n)
            return std::nullopt;
        const std::size_t stop = pos;
        ++pos;
        if(index == target)
            return value_span{ start, stop };
    }
    return std::nullopt;
}

void encode_utf8(unsigned long cp, std::string &out)
{
    if(cp < 0x80u)
        out.push_back(static_cast<char>(cp));
    else if(cp < 0x800u)
    {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
    else if(cp < 0x10000u)
    {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

// Decodes one reference starting at t[amp] == '&', mirroring pugixml's strconv_escape:
// the five named entities plus decimal `&#...;` and lowercase-hex `&#x...;` numeric
// character references. Returns the raw bytes consumed (including '&' and ';') and
// appends the decoded bytes, or 0 when the reference is not one pugixml would decode --
// pugixml then leaves the '&' literal, which the caller reproduces.
std::size_t decode_reference(std::string_view t, std::size_t amp, std::size_t end, std::string &out)
{
    std::size_t j = amp + 1;
    if(j >= end)
        return 0;
    if(t[j] == '#')
    {
        ++j;
        unsigned long code = 0;
        bool any = false;
        if(j < end && t[j] == 'x')
        {
            ++j;
            for(; j < end; ++j)
            {
                const char c = t[j];
                if(c >= '0' && c <= '9')
                    code = code * 16 + static_cast<unsigned long>(c - '0');
                else if((c | 0x20) >= 'a' && (c | 0x20) <= 'f')
                    code = code * 16 + static_cast<unsigned long>((c | 0x20) - 'a' + 10);
                else
                    break;
                any = true;
            }
        }
        else
        {
            for(; j < end; ++j)
            {
                const char c = t[j];
                if(c >= '0' && c <= '9')
                    code = code * 10 + static_cast<unsigned long>(c - '0');
                else
                    break;
                any = true;
            }
        }
        if(!any || j >= end || t[j] != ';')
            return 0;
        encode_utf8(code, out);
        return j + 1 - amp;
    }
    static constexpr struct { std::string_view name; char ch; } named[] = {
        { "amp;", '&' }, { "apos;", '\'' }, { "gt;", '>' }, { "lt;", '<' }, { "quot;", '"' }
    };
    for(const auto &entry : named)
        if(t.substr(j, entry.name.size()) == entry.name)
        {
            out.push_back(entry.ch);
            return j + entry.name.size() - amp;
        }
    return 0;
}

}

int offset_to_line(std::string_view text, std::ptrdiff_t offset)
{
    const std::ptrdiff_t stop = std::min<std::ptrdiff_t>(offset, static_cast<std::ptrdiff_t>(text.size()));
    int line = 1;
    for(std::ptrdiff_t i = 0; i < stop; ++i)
        if(text[static_cast<std::size_t>(i)] == '\n')
            ++line;
    return line;
}

source_location offset_location(std::string_view text, std::ptrdiff_t offset,
                                const std::filesystem::path &file)
{
    const std::ptrdiff_t stop = std::min<std::ptrdiff_t>(offset, static_cast<std::ptrdiff_t>(text.size()));
    int column = 1;
    for(std::ptrdiff_t i = 0; i < stop; ++i)
        column = text[static_cast<std::size_t>(i)] == '\n' ? 1 : column + 1;
    return source_location{ file, offset_to_line(text, offset), column };
}

source_location node_location(pugi::xml_node node, std::string_view text,
                              const std::filesystem::path &file)
{
    return offset_location(text, node.offset_debug(), file);
}

source_location refine_attr_column(std::string_view text, pugi::xml_node host,
                                   std::size_t attr_index, std::size_t decoded_offset,
                                   const source_location &fallback)
{
    const std::ptrdiff_t node_off = host.offset_debug();
    if(node_off < 0 || text.empty() || static_cast<std::size_t>(node_off) >= text.size())
        return fallback;

    const std::optional<value_span> span =
        locate_attr_value(text, static_cast<std::size_t>(node_off), attr_index);
    if(!span)
        return fallback;

    pugi::xml_attribute attr;
    std::size_t idx = 0;
    for(pugi::xml_attribute a : host.attributes())
    {
        if(idx == attr_index)
        {
            attr = a;
            break;
        }
        ++idx;
    }
    if(!attr)
        return fallback;

    // Replay pugixml's three default attribute transforms over the raw value span --
    // parse_escapes, parse_eol, and parse_wconv_attribute (parse_default is
    // parse_cdata|parse_escapes|parse_wconv_attribute|parse_eol) -- accumulating both
    // the decoded string and a decoded-index -> raw-byte-offset map. Whitespace (space,
    // tab, CR, LF) collapses to one space and CRLF to a single space, matching wconv.
    std::string decoded;
    std::vector<std::size_t> raw_at;
    decoded.reserve(span->stop - span->start);
    raw_at.reserve(span->stop - span->start);
    std::size_t i = span->start;
    while(i < span->stop)
    {
        const char c = text[i];
        if(c == '\r')
        {
            raw_at.push_back(i);
            decoded.push_back(' ');
            i += (i + 1 < span->stop && text[i + 1] == '\n') ? std::size_t{2} : std::size_t{1};
        }
        else if(c == '\n' || c == '\t' || c == ' ')
        {
            raw_at.push_back(i);
            decoded.push_back(' ');
            ++i;
        }
        else if(c == '&')
        {
            std::string bytes;
            const std::size_t consumed = decode_reference(text, i, span->stop, bytes);
            if(consumed == 0)
            {
                raw_at.push_back(i);
                decoded.push_back('&');
                ++i;
            }
            else
            {
                for(char b : bytes)
                {
                    raw_at.push_back(i);
                    decoded.push_back(b);
                }
                i += consumed;
            }
        }
        else
        {
            raw_at.push_back(i);
            decoded.push_back(c);
            ++i;
        }
    }

    // The self-check backstop: only a decode that reproduces
    // attr.value() byte-for-byte proves this index map mirrors pugixml for this value.
    // On any divergence -- or a token index the map cannot place -- degrade to the
    // value-start column rather than emit a column the self-check did not confirm; a
    // confidently-wrong column is worse than a coarse-but-correct one.
    if(std::string_view(decoded) != std::string_view(attr.value()) || decoded_offset >= raw_at.size())
        return offset_location(text, static_cast<std::ptrdiff_t>(span->start), fallback.file);
    return offset_location(text, static_cast<std::ptrdiff_t>(raw_at[decoded_offset]), fallback.file);
}

}

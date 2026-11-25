#include "attr_value_replay.h"

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <optional>
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

std::size_t skip_space(std::string_view t, std::size_t pos)
{
    while(pos < t.size() && is_xml_space(t[pos]))
        ++pos;
    return pos;
}

std::size_t skip_element_name(std::string_view t, std::size_t pos)
{
    while(pos < t.size() && !is_xml_space(t[pos]) && t[pos] != '/' && t[pos] != '>')
        ++pos;
    return pos;
}

// The scan is bounded only by the matching quote: the other quote character and even '>' may legally appear inside a value.
std::optional<value_span> next_attr_value(std::string_view t, std::size_t pos)
{
    const std::size_t n = t.size();
    pos                 = skip_space(t, pos);
    if(pos >= n || t[pos] == '/' || t[pos] == '>')
        return std::nullopt;
    while(pos < n && t[pos] != '=' && !is_xml_space(t[pos]))
        ++pos;
    pos = skip_space(t, pos);
    if(pos >= n || t[pos] != '=')
        return std::nullopt;
    pos = skip_space(t, pos + 1);
    if(pos >= n || (t[pos] != '"' && t[pos] != '\''))
        return std::nullopt;
    const char quote        = t[pos];
    const std::size_t start = pos + 1;
    std::size_t stop        = start;
    while(stop < n && t[stop] != quote)
        ++stop;
    if(stop >= n)
        return std::nullopt;
    return value_span{start, stop};
}

std::optional<value_span> locate_attr_value(std::string_view t, std::size_t pos, std::size_t target)
{
    pos = skip_element_name(t, pos);
    for(std::size_t index = 0; pos < t.size(); ++index)
    {
        const std::optional<value_span> span = next_attr_value(t, pos);
        if(!span || index == target)
            return span;
        pos = span->stop + 1;
    }
    return std::nullopt;
}

void encode_utf8(std::uint32_t cp, std::string &out)
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

std::int32_t digit_value(char c, bool hex)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(hex && (c | 0x20) >= 'a' && (c | 0x20) <= 'f')
        return (c | 0x20) - 'a' + 10;
    return -1;
}

std::size_t decode_numeric(std::string_view t, std::size_t amp, std::size_t end, std::string &out)
{
    const bool hex           = amp + 2 < end && t[amp + 2] == 'x';
    const std::size_t digits = hex ? amp + 3 : amp + 2;
    std::size_t j            = digits;
    std::uint32_t code       = 0;
    for(; j < end && digit_value(t[j], hex) >= 0; ++j)
        code = code * (hex ? 16 : 10) + static_cast<std::uint32_t>(digit_value(t[j], hex));
    if(j == digits || j >= end || t[j] != ';')
        return 0;
    encode_utf8(code, out);
    return j + 1 - amp;
}

std::size_t decode_named(std::string_view t, std::size_t amp, std::string &out)
{
    static constexpr struct
    {
        std::string_view name;
        char ch;
    } named[] = {{"amp;", '&'}, {"apos;", '\''}, {"gt;", '>'}, {"lt;", '<'}, {"quot;", '"'}};
    for(const auto &entry : named)
        if(t.substr(amp + 1, entry.name.size()) == entry.name)
        {
            out.push_back(entry.ch);
            return entry.name.size() + 1;
        }
    return 0;
}

// Mirrors pugixml's strconv_escape: the five named entities plus decimal and hex numeric references. Returns the raw
// bytes consumed, or 0 for a reference pugixml leaves undecoded, in which case it keeps the '&' literal.
std::size_t decode_reference(std::string_view t, std::size_t amp, std::size_t end, std::string &out)
{
    if(amp + 1 >= end)
        return 0;
    if(t[amp + 1] == '#')
        return decode_numeric(t, amp, end, out);
    return decode_named(t, amp, out);
}

void emit(replayed_value &out, std::size_t raw, std::string_view bytes)
{
    for(char b : bytes)
    {
        out.raw_at.push_back(raw);
        out.decoded.push_back(b);
    }
}

std::size_t replay_reference(std::string_view t, std::size_t i, std::size_t stop, replayed_value &out)
{
    std::string bytes;
    const std::size_t consumed = decode_reference(t, i, stop, bytes);
    emit(out, i, consumed == 0 ? std::string_view("&") : std::string_view(bytes));
    return consumed == 0 ? std::size_t{1} : consumed;
}

std::size_t replay_step(std::string_view t, std::size_t i, std::size_t stop, replayed_value &out)
{
    const char c = t[i];
    if(c == '&')
        return replay_reference(t, i, stop, out);
    emit(out, i, is_xml_space(c) ? std::string_view(" ") : t.substr(i, 1));
    if(c == '\r' && i + 1 < stop && t[i + 1] == '\n')
        return std::size_t{2};
    return std::size_t{1};
}

}

std::optional<replayed_value> replay_attr_value(std::string_view text, std::size_t tag_offset, std::size_t attr_index)
{
    const std::optional<value_span> span = locate_attr_value(text, tag_offset, attr_index);
    if(!span)
        return std::nullopt;
    replayed_value out{span->start, {}, {}};
    out.decoded.reserve(span->stop - span->start);
    out.raw_at.reserve(span->stop - span->start);
    for(std::size_t i = span->start; i < span->stop;)
        i += replay_step(text, i, span->stop, out);
    return out;
}

}

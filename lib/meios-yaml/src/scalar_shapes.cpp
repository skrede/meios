#include "scalar_shapes.h"

#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{
namespace
{

bool decimal(char c)
{
    return c >= '0' && c <= '9';
}

bool octal(char c)
{
    return c >= '0' && c <= '7';
}

bool binary(char c)
{
    return c == '0' || c == '1';
}

bool hexadecimal(char c)
{
    return decimal(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

std::size_t signless(std::string_view text)
{
    return !text.empty() && (text[0] == '-' || text[0] == '+') ? 1 : 0;
}

std::size_t past_digits(std::string_view text, std::size_t at)
{
    while(at < text.size() && (decimal(text[at]) || text[at] == '_'))
        ++at;
    return at;
}

// A digit run in which an underscore may appear anywhere, matching the character classes of
// the upstream patterns rather than what the conversion beneath them will accept.
template <typename Member>
bool run_of(std::string_view text, std::size_t at, Member member)
{
    if(at >= text.size())
        return false;
    for(; at < text.size(); ++at)
        if(text[at] != '_' && !member(text[at]))
            return false;
    return true;
}

// The exponent the upstream float pattern admits carries a mandatory sign, which is the whole
// reason a plain 1e5 resolves as text rather than as a number.
bool exponent_or_end(std::string_view text, std::size_t at)
{
    if(at == text.size())
        return true;
    if(text[at] != 'e' && text[at] != 'E')
        return false;
    ++at;
    if(at == text.size() || (text[at] != '+' && text[at] != '-'))
        return false;
    ++at;
    if(at == text.size())
        return false;
    for(; at < text.size(); ++at)
        if(!decimal(text[at]))
            return false;
    return true;
}

// A colon group is one or two digits, the leading one no greater than five.
bool minutes_run(std::string_view text, std::size_t &at)
{
    const std::size_t start = at;
    while(at < text.size() && decimal(text[at]))
        ++at;
    if(at == start || at - start > 2)
        return false;
    return at - start == 1 || (text[start] >= '0' && text[start] <= '5');
}

}

std::optional<bool> boolean_shape(std::string_view text)
{
    static constexpr std::array<std::string_view, 9> truths{ "yes", "Yes", "YES", "true", "True",
                                                             "TRUE", "on", "On", "ON" };
    static constexpr std::array<std::string_view, 9> falsehoods{
        "no", "No", "NO", "false", "False", "FALSE", "off", "Off", "OFF"
    };
    if(std::ranges::find(truths, text) != truths.end())
        return true;
    if(std::ranges::find(falsehoods, text) != falsehoods.end())
        return false;
    return std::nullopt;
}

bool null_shape(std::string_view text)
{
    return text.empty() || text == "~" || text == "null" || text == "Null" || text == "NULL";
}

bool float_shape(std::string_view text)
{
    std::size_t at = signless(text);
    if(at < text.size() && decimal(text[at]))
    {
        at = past_digits(text, at + 1);
        if(at == text.size() || text[at] != '.')
            return false;
        return exponent_or_end(text, past_digits(text, at + 1));
    }
    // The leading-dot spelling admits no sign upstream, so a signed one is not a float.
    if(at != 0 || at + 1 >= text.size() || text[at] != '.' || !decimal(text[at + 1]))
        return false;
    return exponent_or_end(text, past_digits(text, at + 2));
}

bool non_finite_shape(std::string_view text)
{
    const std::string_view body = text.substr(signless(text));
    if(body == ".inf" || body == ".Inf" || body == ".INF")
        return true;
    return text == ".nan" || text == ".NaN" || text == ".NAN";
}

bool sexagesimal_shape(std::string_view text)
{
    std::size_t at = signless(text);
    if(at == text.size() || !decimal(text[at]))
        return false;
    at = past_digits(text, at + 1);
    if(at == text.size() || text[at] != ':')
        return false;
    while(at < text.size() && text[at] == ':')
    {
        ++at;
        if(!minutes_run(text, at))
            return false;
    }
    if(at < text.size() && text[at] == '.')
        at = past_digits(text, at + 1);
    return at == text.size();
}

std::optional<integer_shape> integer_spelling(std::string_view text)
{
    const std::size_t at = signless(text);
    const std::string_view body = text.substr(at);
    if(body.starts_with("0b") && run_of(body, 2, binary))
        return integer_shape{ 2, at + 2 };
    if(body.starts_with("0x") && run_of(body, 2, hexadecimal))
        return integer_shape{ 16, at + 2 };
    if(body.size() > 1 && body[0] == '0' && run_of(body, 1, octal))
        return integer_shape{ 8, at };
    if(body == "0")
        return integer_shape{ 10, at };
    if(!body.empty() && body[0] >= '1' && body[0] <= '9' && run_of(body, 0, decimal))
        return integer_shape{ 10, at };
    return std::nullopt;
}

}

#include "structural_detail.h"

#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

bool is_separating_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

char step_quote(char open, char c)
{
    if(open != '\0')
        return c == open ? '\0' : open;
    if(c == '\'' || c == '"')
        return c;
    return open;
}

std::size_t token_end(std::string_view spec, std::size_t from)
{
    char open = '\0';
    for(std::size_t i = from; i < spec.size(); ++i)
    {
        open = step_quote(open, spec[i]);
        if(open == '\0' && is_separating_space(spec[i]))
            return i;
    }
    return std::string_view::npos;
}

std::size_t separator_at(std::string_view token)
{
    char open = '\0';
    for(std::size_t i = 0; i < token.size(); ++i)
    {
        open = step_quote(open, token[i]);
        if(open == '\0' && token[i] == '=')
            return i > 0 && token[i - 1] == ':' ? i - 1 : i;
    }
    return std::string_view::npos;
}

std::size_t separator_width(std::string_view token, std::size_t at)
{
    return token[at] == ':' ? std::size_t{ 2 } : std::size_t{ 1 };
}

std::string_view unquote(std::string_view text)
{
    if(text.size() < 2 || text.front() != text.back())
        return text;
    if(text.front() != '\'' && text.front() != '"')
        return text;
    return text.substr(1, text.size() - 2);
}

// A `^|` fallback's quotes sit after the marker, so the pair to remove wraps the
// fallback portion rather than the whole default text.
std::string default_text(std::string_view raw)
{
    if(raw.rfind("^|", 0) != 0)
        return std::string(unquote(raw));
    return "^|" + std::string(unquote(raw.substr(2)));
}

bool parse_block(std::string_view token, macro_def &def)
{
    if(token.front() != '*')
        return false;
    const bool children = token.rfind("**", 0) == 0;
    const std::size_t marker = children ? std::size_t{ 2 } : std::size_t{ 1 };
    def.block_params.emplace_back(token.substr(marker));
    def.block_children.push_back(children);
    return true;
}

void parse_param(std::string_view token, macro_def &def)
{
    if(parse_block(token, def))
        return;
    const std::size_t at = separator_at(token);
    if(at == std::string_view::npos)
    {
        def.params.emplace_back(token);
        def.defaults.emplace_back(std::nullopt);
        return;
    }
    def.params.emplace_back(token.substr(0, at));
    def.defaults.emplace_back(default_text(token.substr(at + separator_width(token, at))));
}

}

void parse_params(std::string_view spec, macro_def &def)
{
    for(std::size_t i = 0; i < spec.size();)
    {
        const std::size_t end = token_end(spec, i);
        std::string_view token = spec.substr(i, end == std::string_view::npos ? end : end - i);
        if(!token.empty())
            parse_param(token, def);
        if(end == std::string_view::npos)
            break;
        i = end + 1;
    }
}

}

#include "macro_param_scan.h"
#include "structural_detail.h"

#include <string>
#include <cstddef>
#include <utility>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

constexpr std::string_view forward_markers[] = { "^|", "^", "" };

struct param_spec
{
    std::string_view name;
    std::optional<std::string> written;
    std::size_t rest;
    std::size_t limit;
    bool writes_default;
};

std::string_view unquote(std::string_view text)
{
    if(text.size() < 2 || text.front() != text.back() || (text.front() != '\'' && text.front() != '"'))
        return text;
    return text.substr(1, text.size() - 2);
}

// An empty default is no default, and a forward marker with nothing after it inherits or refuses.
std::optional<std::string> written_default(std::string_view forward, std::string_view text)
{
    if(forward.empty())
        return text.empty() ? std::nullopt : std::optional<std::string>(unquote(text));
    return text.empty() ? std::string("^") : "^|" + std::string(unquote(text));
}

// After a spec that matched a default, the rest of the list ends at its next newline, and what follows is dropped.
std::size_t rest_limit(std::string_view spec, std::size_t rest)
{
    return std::min(spec.find('\n', skip_space(spec, rest)), spec.size());
}

std::optional<param_spec> read_default(std::string_view spec, std::string_view name, std::size_t from)
{
    std::size_t at = skip_space(spec, from);
    if(at < spec.size() && spec[at] == ':')
        ++at;
    if(at == spec.size() || spec[at] != '=')
        return std::nullopt;
    const std::size_t value = skip_space(spec, at + 1);
    for(std::string_view forward : forward_markers)
    {
        const std::size_t text = value + forward.size();
        if(!spec.substr(value).starts_with(forward))
            continue;
        if(std::optional<std::size_t> end = default_end(spec, text))
            return param_spec{ name, written_default(forward, spec.substr(text, *end - text)), *end, rest_limit(spec, *end), true };
    }
    if(value > at + 1)
        return param_spec{ name, std::nullopt, value, rest_limit(spec, value), true };
    return std::nullopt;
}

// One spec at a time, as `re_macro_arg` and `parse_macro_arg` read it in xacro 2.1.1: a spec the
// pattern does not match is its first whitespace-delimited token, a name with no default.
param_spec read_spec(std::string_view spec, std::size_t at)
{
    std::size_t end = at;
    while(!breaks_at(spec, end) && spec[end] != ':' && spec[end] != '=')
        ++end;
    if(end > at)
        if(std::optional<param_spec> read = read_default(spec, spec.substr(at, end - at), end))
            return *read;
    end = at;
    while(!breaks_at(spec, end))
        ++end;
    return param_spec{ spec.substr(at, end - at), std::nullopt, end, spec.size(), false };
}

bool parse_block(std::string_view name, macro_def &def)
{
    if(name.front() != '*')
        return false;
    const bool children = name.rfind("**", 0) == 0;
    const std::size_t marker = children ? std::size_t{ 2 } : std::size_t{ 1 };
    def.block_params.emplace_back(name.substr(marker));
    def.block_children.push_back(children);
    return true;
}

}

bool parse_params(std::string_view spec, macro_def &def)
{
    if(!spec.empty() && skip_space(spec, 0) == spec.size())
        return false;
    for(std::size_t at = skip_space(spec, 0); at < spec.size(); at = skip_space(spec, at))
    {
        param_spec read = read_spec(spec, at);
        at = read.rest;
        spec = spec.substr(0, read.limit);
        if(parse_block(read.name, def))
            continue;
        def.params.emplace_back(read.name);
        def.defaults.push_back(std::move(read.written));
        def.writes_default.push_back(read.writes_default);
    }
    return true;
}

}

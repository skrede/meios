#include "structural_detail.h"

#include <string>
#include <vector>
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

bool is_separating_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool breaks_at(std::string_view spec, std::size_t at)
{
    return at == spec.size() || is_separating_space(spec[at]);
}

std::size_t skip_space(std::string_view spec, std::size_t at)
{
    while(at < spec.size() && is_separating_space(spec[at]))
        ++at;
    return at;
}

std::optional<std::size_t> span_end(std::string_view spec, std::size_t at, std::string_view opener, char closer)
{
    if(!spec.substr(at).starts_with(opener))
        return std::nullopt;
    for(std::size_t k = at + opener.size(); k < spec.size() && spec[k] != '\n'; ++k)
        if(spec[k] == closer && breaks_at(spec, k + 1))
            return k + 1;
    return std::nullopt;
}

std::size_t next_close(std::string_view spec, std::size_t open, std::size_t after)
{
    for(std::size_t k = after + 1; k < spec.size() && spec[k] != '\n'; ++k)
        if(spec[k] == spec[open])
            return k;
    return std::string_view::npos;
}

// Resumes at the nearest untried close of the innermost open quote, abandoning a quote once it has
// no close left; a quote abandoned once leads nowhere from any path, so it is not retried.
std::optional<std::size_t> retry(std::string_view spec, std::vector<std::pair<std::size_t, std::size_t>> &quoted,
                                 std::vector<bool> &dead)
{
    while(!quoted.empty())
    {
        auto &[open, close] = quoted.back();
        close = next_close(spec, open, close);
        if(close != std::string_view::npos)
            return close + 1;
        dead[open] = true;
        quoted.pop_back();
    }
    return std::nullopt;
}

// A run of quoted strings and bare characters ending at whitespace or the end, backtracked as the
// reference's regular expression backtracks it: a quoted string may close at any later matching
// quote on its line, the nearest tried first.
std::optional<std::size_t> run_end(std::string_view spec, std::size_t at)
{
    std::vector<std::pair<std::size_t, std::size_t>> quoted;
    std::vector<bool> dead(spec.size() + 1, false);
    for(std::optional<std::size_t> q = at; q; q = retry(spec, quoted, dead))
    {
        std::size_t k = *q;
        while(!breaks_at(spec, k) && spec[k] != '\'' && spec[k] != '"')
            ++k;
        if(breaks_at(spec, k) && k > at)
            return k;
        if(!breaks_at(spec, k) && !dead[k])
            quoted.emplace_back(k, k);
    }
    return std::nullopt;
}

std::optional<std::size_t> default_end(std::string_view spec, std::size_t at)
{
    if(std::optional<std::size_t> end = span_end(spec, at, "${", '}'))
        return end;
    if(std::optional<std::size_t> end = span_end(spec, at, "$(", ')'))
        return end;
    if(std::optional<std::size_t> end = run_end(spec, at))
        return end;
    return breaks_at(spec, at) ? std::optional<std::size_t>(at) : std::nullopt;
}

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

// The reference's pattern ends in `(.*)`, which stops at a newline, so what follows a matched spec's first newline is dropped.
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
    while(end < spec.size() && !is_separating_space(spec[end]) && spec[end] != ':' && spec[end] != '=')
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

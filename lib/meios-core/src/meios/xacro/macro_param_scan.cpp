#include "python_space.h"
#include "macro_param_scan.h"

#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

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

// A run of quoted strings and bare characters ending at whitespace or the end, backtracked as a
// regular expression would backtrack it: a quoted string may close at any later matching quote on
// its line, the nearest tried first.
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

}

bool breaks_at(std::string_view spec, std::size_t at)
{
    return at == spec.size() || python_space_at(spec, at) > 0;
}

std::size_t skip_space(std::string_view spec, std::size_t at)
{
    while(const std::size_t length = python_space_at(spec, at))
        at += length;
    return at;
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

}

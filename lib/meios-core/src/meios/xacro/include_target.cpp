#include "include_target.h"

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

constexpr std::string_view separators = std::filesystem::path::preferred_separator == '\\' ? std::string_view("/\\") : std::string_view("/");

}

include_target split_target(std::string_view raw)
{
    if(raw.rfind("package://", 0) == 0)
    {
        std::string_view rest = raw.substr(10);
        std::size_t slash = rest.find('/');
        if(slash == std::string_view::npos)
            return { std::string(rest), {}, std::nullopt };
        return { std::string(rest.substr(0, slash)), std::string(rest.substr(slash + 1)), std::nullopt };
    }
    std::size_t close = raw.rfind("$(find ", 0) == 0 ? raw.find(')') : std::string_view::npos;
    if(close == std::string_view::npos)
        return { {}, std::string(raw), std::nullopt };
    std::string_view after = raw.substr(close + 1);
    if(!after.empty() && after.front() == '/')
        after.remove_prefix(1);
    return { std::string(raw.substr(7, close - 7)), std::string(after), close };
}

std::string normalize_relative(std::string_view rel, bool &escaped)
{
    std::vector<std::string_view> parts;
    escaped = false;
    for(std::size_t i = 0; i <= rel.size();)
    {
        std::size_t slash = rel.find_first_of(separators, i);
        std::string_view seg = rel.substr(i, slash == std::string_view::npos ? slash : slash - i);
        if(seg == "..")
        {
            if(parts.empty())
                return (escaped = true, std::string{});
            parts.pop_back();
        }
        else if(!seg.empty() && seg != ".")
            parts.push_back(seg);
        if(slash == std::string_view::npos)
            break;
        i = slash + 1;
    }
    std::string out;
    for(std::string_view part : parts)
        out += (out.empty() ? "" : "/") + std::string(part);
    return out;
}

std::filesystem::path located_key(const std::filesystem::path &located)
{
    std::error_code canon_ec;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(located, canon_ec);
    return canon_ec ? located : canonical;
}

// A spelling with no package is named as the document wrote it, because its normalized form
// joined onto an empty package reads as an absolute path nobody wrote.
std::string unresolved_message(const std::string &package, const std::string &normalized, const std::string &written, bool looked_beside)
{
    if(!package.empty())
        return "xacro:include could not resolve \"" + package + '/' + normalized + '"';
    const std::string_view beside = looked_beside ? "beside the document that writes it or " : "";
    return "xacro:include could not resolve \"" + written + "\": looked for it " + std::string(beside) + "in the configured sources";
}

}

#include "include_target.h"

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

struct element
{
    std::string_view text;
    std::size_t end;
};

std::vector<element> elements(std::string_view text, const path_grammar &grammar)
{
    std::vector<element> out;
    for(std::size_t i = 0; i <= text.size();)
    {
        const std::size_t slash = std::min(text.find_first_of(grammar.separators, i), text.size());
        const std::string_view seg = text.substr(i, slash - i);
        if(!seg.empty() && seg != ".")
            out.push_back(element{ seg, slash });
        i = slash + 1;
    }
    return out;
}

std::string joined(const std::vector<std::string_view> &parts)
{
    std::string out;
    for(std::string_view part : parts)
        out += (out.empty() ? "" : "/") + std::string(part);
    return out;
}

std::string unrooted(std::string text, const path_grammar &grammar)
{
    for(std::size_t root = root_length(text, grammar); root != 0; root = root_length(text, grammar))
        text.erase(0, root);
    return text;
}

std::string lookups_text(const include_lookups &asked)
{
    if(!asked.served_package)
        return asked.beside ? "beside the document that writes it or in the configured sources" : "in the configured sources";
    const std::string package = "in the package \"" + *asked.served_package + '"';
    if(!asked.beside)
        return package + " that served the document that writes it, then in the configured sources";
    return "beside the document that writes it, then " + package + " that served that document, then in the configured sources";
}

}

include_target split_target(std::string_view raw)
{
    if(raw.rfind("package://", 0) == 0)
    {
        std::string_view rest = raw.substr(10);
        std::size_t slash = rest.find('/');
        if(slash == std::string_view::npos)
            return { std::string(rest), {} };
        return { std::string(rest.substr(0, slash)), std::string(rest.substr(slash + 1)) };
    }
    return { {}, std::string(raw) };
}

bool is_separator(char c)
{
    return native_paths.separators.find(c) != std::string_view::npos;
}

std::optional<std::size_t> directory_prefix(std::string_view text, std::string_view directory, const path_grammar &grammar)
{
    const std::vector<element> held    = elements(directory, grammar);
    const std::vector<element> written = elements(text, grammar);
    if(held.empty() || written.size() < held.size() || rooted(text, grammar) != rooted(directory, grammar))
        return std::nullopt;
    for(std::size_t i = 0; i < held.size(); ++i)
        if(written[i].text != held[i].text)
            return std::nullopt;
    return written[held.size() - 1].end;
}

std::string normalize_relative(std::string_view rel, bool &escaped, const path_grammar &grammar)
{
    std::vector<std::string_view> parts;
    escaped = false;
    for(const element &each : elements(rel.substr(root_length(rel, grammar)), grammar))
    {
        if(each.text != "..")
            parts.push_back(each.text);
        else if(parts.empty())
            return (escaped = true, std::string{});
        else
            parts.pop_back();
    }
    return unrooted(joined(parts), grammar);
}

std::filesystem::path located_key(const std::filesystem::path &located)
{
    std::error_code canon_ec;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(located, canon_ec);
    return canon_ec ? located : canonical;
}

// A spelling with no package is named as the document wrote it, because its normalized form
// joined onto an empty package reads as an absolute path nobody wrote.
std::string unresolved_message(const std::string &package, const std::string &normalized, const std::string &written, const include_lookups &asked)
{
    if(!package.empty())
        return "xacro:include could not resolve \"" + package + '/' + normalized + '"';
    return "xacro:include could not resolve \"" + written + "\": looked for it " + lookups_text(asked);
}

}

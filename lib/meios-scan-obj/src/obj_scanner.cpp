#include "meios/scan/obj_scanner.h"

#include "meios/bundle/asset_bytes.h"

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>
#include <cctype>
#include <sstream>
#include <string_view>

namespace meios::detail
{

std::string_view trim(std::string_view line)
{
    while(!line.empty() && (line.front() == ' ' || line.front() == '\t'))
        line.remove_prefix(1);
    while(!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
        line.remove_suffix(1);
    return line;
}

std::vector<std::string_view> split(std::string_view line)
{
    std::vector<std::string_view> tokens;
    std::size_t pos = 0;
    while(pos < line.size())
    {
        while(pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
            ++pos;
        const std::size_t start = pos;
        while(pos < line.size() && line[pos] != ' ' && line[pos] != '\t')
            ++pos;
        if(pos > start)
            tokens.push_back(line.substr(start, pos - start));
    }
    return tokens;
}

std::string lower(std::string_view text)
{
    std::string out(text);
    for(char &c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool is_texture_statement(std::string_view keyword)
{
    static const std::string_view kinds[] = {
        "map_ka", "map_kd", "map_ks", "map_ns", "map_d", "map_bump", "bump",
        "disp", "decal", "refl", "map_pr", "map_pm", "map_ps", "norm" };
    for(std::string_view kind : kinds)
        if(keyword == kind)
            return true;
    return false;
}

void collect_refs(std::string_view raw, std::vector<std::string> &refs)
{
    const std::string_view line = trim(raw);
    if(line.empty() || line.front() == '#')
        return;
    const std::vector<std::string_view> tokens = split(line);
    if(tokens.empty())
        return;
    const std::string keyword = lower(tokens.front());
    if(keyword == "mtllib")
        for(std::size_t i = 1; i < tokens.size(); ++i)
            refs.emplace_back(tokens[i]);
    else if(tokens.size() >= 2 && is_texture_statement(keyword))
        refs.emplace_back(tokens.back());
}

}

namespace meios
{

std::vector<std::string> obj_scanner::scan(const resolved_asset &asset, log_sink &log)
{
    const std::string text = read_asset_text(asset, log);
    std::vector<std::string> refs;
    std::istringstream in(text);
    std::string line;
    while(std::getline(in, line))
        detail::collect_refs(line, refs);
    return refs;
}

}

#include "meios/scan/gltf_scanner.h"

#include "glb.h"

#include "meios/bundle/asset_bytes.h"

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

using json = nlohmann::json;

int hex_value(char c)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string percent_decode(std::string_view uri)
{
    std::string out;
    out.reserve(uri.size());
    for(std::size_t i = 0; i < uri.size(); ++i)
    {
        const int hi = i + 2 < uri.size() ? hex_value(uri[i + 1]) : -1;
        const int lo = i + 2 < uri.size() ? hex_value(uri[i + 2]) : -1;
        if(uri[i] == '%' && hi >= 0 && lo >= 0)
        {
            out.push_back(static_cast<char>(hi * 16 + lo));
            i += 2;
        }
        else
            out.push_back(uri[i]);
    }
    return out;
}

bool is_external(std::string_view uri)
{
    return !uri.empty() && !uri.starts_with("data:");
}

void collect_uris(const json &doc, const char *key, std::vector<std::string> &refs)
{
    const json::const_iterator section = doc.find(key);
    if(section == doc.end() || !section->is_array())
        return;
    for(const json &item : *section)
    {
        const json::const_iterator uri = item.find("uri");
        if(uri == item.end() || !uri->is_string())
            continue;
        const std::string value = uri->get<std::string>();
        if(is_external(value))
            refs.push_back(percent_decode(value));
    }
}

}

std::vector<std::string> scan_gltf_json(std::string_view text, log_sink &log)
{
    const json doc = json::parse(text.begin(), text.end(), nullptr, false);
    if(doc.is_discarded())
    {
        log.log(level::error, "gltf: JSON parse error");
        return {};
    }
    std::vector<std::string> refs;
    collect_uris(doc, "images", refs);
    collect_uris(doc, "buffers", refs);
    return refs;
}

}

namespace meios
{

std::vector<std::string> gltf_scanner::scan(const resolved_asset &asset, log_sink &log)
{
    const std::string bytes = read_asset_text(asset, log);
    if(!detail::looks_like_glb(bytes))
        return detail::scan_gltf_json(bytes, log);
    const std::optional<std::string> json = detail::extract_glb_json(bytes, log);
    if(!json)
        return {};
    return detail::scan_gltf_json(*json, log);
}

}

#include "meios/scan/gltf_scanner.h"

#include "glb.h"

#include "meios/bundle/asset_bytes.h"

#include "meios/io/uri_decode.h"
#include "meios/io/text_reader.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

using json = nlohmann::json;

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

// Refs are emitted verbatim (decoded, possibly carrying `..` or absolute segments);
// containment against the bundle root is enforced downstream by the package/zip
// writer's contained_candidate guard, so this scanner must not pre-filter them.
std::vector<std::string> gltf_scanner::scan(const resolved_asset &asset, log_sink &log)
{
    const text_read_result bytes = read_asset_text(asset, log);
    if(!bytes)
        return {};
    if(!detail::looks_like_glb(*bytes))
        return detail::scan_gltf_json(*bytes, log);
    const std::optional<std::string> json = detail::extract_glb_json(*bytes, log);
    if(!json)
        return {};
    return detail::scan_gltf_json(*json, log);
}

}

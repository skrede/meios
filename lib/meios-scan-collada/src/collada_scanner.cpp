#include "meios/scan/collada_scanner.h"

#include "meios/bundle/asset_bytes.h"

#include "meios/io/uri_decode.h"
#include "meios/io/text_reader.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <string_view>

namespace meios::detail
{

namespace
{

std::string_view trim(std::string_view text)
{
    const std::string_view space = " \t\r\n\f\v";
    const std::size_t first = text.find_first_not_of(space);
    if(first == std::string_view::npos)
        return {};
    return text.substr(first, text.find_last_not_of(space) - first + 1);
}

std::string normalize_ref(std::string_view ref)
{
    ref = trim(ref);
    if(ref.starts_with("file://"))
        ref.remove_prefix(7);
    return percent_decode(ref);
}

void append_ref(pugi::xml_node init_from, std::vector<std::string> &refs)
{
    std::string_view text = init_from.text().get();
    if(trim(text).empty())
        text = init_from.child("ref").text().get();
    std::string ref = normalize_ref(text);
    if(!ref.empty())
        refs.push_back(std::move(ref));
}

void collect_init_from(pugi::xml_node node, std::vector<std::string> &refs)
{
    for(pugi::xml_node child : node.children())
    {
        if(std::string_view(child.name()) == "init_from")
            append_ref(child, refs);
        collect_init_from(child, refs);
    }
}

}

}

namespace meios
{

// Refs are emitted verbatim (decoded, possibly carrying `..` or absolute segments);
// containment against the bundle root is enforced downstream by the package/zip
// writer's contained_candidate guard, so this scanner must not pre-filter them.
std::vector<std::string> collada_scanner::scan(const resolved_asset &asset, log_sink &log)
{
    const text_read_result text = read_asset_text(asset, log);
    if(!text)
        return {};
    pugi::xml_document doc;
    const pugi::xml_parse_result parsed = doc.load_buffer(text->data(), text->size());
    if(!parsed)
    {
        log.log(level::error, std::string("collada parse error: ") + parsed.description());
        return {};
    }
    std::vector<std::string> refs;
    detail::collect_init_from(doc, refs);
    return refs;
}

}

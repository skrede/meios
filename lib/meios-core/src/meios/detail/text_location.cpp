#include "text_location.h"
#include "attr_value_replay.h"

#include <string>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

pugi::xml_attribute attribute_at(pugi::xml_node host, std::size_t attr_index)
{
    std::size_t idx = 0;
    for(pugi::xml_attribute a : host.attributes())
        if(idx++ == attr_index)
            return a;
    return pugi::xml_attribute();
}

}

int offset_to_line(std::string_view text, std::ptrdiff_t offset)
{
    const std::ptrdiff_t stop = std::min<std::ptrdiff_t>(offset, static_cast<std::ptrdiff_t>(text.size()));
    int line                  = 1;
    for(std::ptrdiff_t i = 0; i < stop; ++i)
        if(text[static_cast<std::size_t>(i)] == '\n')
            ++line;
    return line;
}

source_location offset_location(std::string_view text, std::ptrdiff_t offset, const std::filesystem::path &file)
{
    const std::ptrdiff_t stop = std::min<std::ptrdiff_t>(offset, static_cast<std::ptrdiff_t>(text.size()));
    int column                = 1;
    for(std::ptrdiff_t i = 0; i < stop; ++i)
        column = text[static_cast<std::size_t>(i)] == '\n' ? 1 : column + 1;
    return source_location{file, offset_to_line(text, offset), column};
}

source_location node_location(pugi::xml_node node, std::string_view text, const std::filesystem::path &file)
{
    return offset_location(text, node.offset_debug(), file);
}

source_location refine_attr_column(std::string_view text, pugi::xml_node host, std::size_t attr_index, std::size_t decoded_offset, const source_location &fallback)
{
    const std::ptrdiff_t node_off = host.offset_debug();
    if(node_off < 0 || text.empty() || static_cast<std::size_t>(node_off) >= text.size())
        return fallback;
    const std::optional<replayed_value> value = replay_attr_value(text, static_cast<std::size_t>(node_off), attr_index);
    const pugi::xml_attribute attr            = attribute_at(host, attr_index);
    if(!value || !attr)
        return fallback;
    // Only a decode that reproduces attr.value() byte for byte proves the map mirrors pugixml for this value; on a
    // divergence, or an offset the map cannot place, a coarse value-start column beats a confidently wrong one.
    if(std::string_view(value->decoded) != std::string_view(attr.value()) || decoded_offset >= value->raw_at.size())
        return offset_location(text, static_cast<std::ptrdiff_t>(value->start), fallback.file);
    return offset_location(text, static_cast<std::ptrdiff_t>(value->raw_at[decoded_offset]), fallback.file);
}

}

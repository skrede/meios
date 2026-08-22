#ifndef HPP_GUARD_MEIOS_DETAIL_TEXT_LOCATION_H
#define HPP_GUARD_MEIOS_DETAIL_TEXT_LOCATION_H

#include "meios/diagnostic/source_location.h"

#include <pugixml.hpp>

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

int offset_to_line(std::string_view text, std::ptrdiff_t offset);

source_location offset_location(std::string_view text, std::ptrdiff_t offset,
                                const std::filesystem::path &file);

source_location node_location(pugi::xml_node node, std::string_view text,
                              const std::filesystem::path &file);

// Refines a node-anchored location to the real document column of a failing token
// inside an attribute value. `text` is the raw source of the hosting document,
// `host` the element carrying the attribute (read for offset_debug and the attribute
// handle at DOM position `attr_index`), and `decoded_offset` the token's index into
// the decoded attribute value. On a self-check-confirmed decode the returned column
// is the token column; on a decode divergence it degrades to the value-start column;
// when the attribute cannot be located in the raw tag it returns `fallback` unchanged.
source_location refine_attr_column(std::string_view text, pugi::xml_node host,
                                   std::size_t attr_index, std::size_t decoded_offset,
                                   const source_location &fallback);

}

#endif

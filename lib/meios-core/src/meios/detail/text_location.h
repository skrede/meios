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

}

#endif

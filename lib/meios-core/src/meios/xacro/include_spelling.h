#ifndef HPP_GUARD_MEIOS_XACRO_INCLUDE_SPELLING_H
#define HPP_GUARD_MEIOS_XACRO_INCLUDE_SPELLING_H

#include "expand_context.h"
#include "include_target.h"

#include <pugixml.hpp>

#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

// Empty when the filename refused, which the expansion has recorded.
std::optional<include_target> spell_include(expand_ctx &ctx, pugi::xml_node in, std::string_view raw, const std::filesystem::path &document);

}

#endif

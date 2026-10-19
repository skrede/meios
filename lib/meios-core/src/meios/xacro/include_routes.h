#ifndef HPP_GUARD_MEIOS_XACRO_INCLUDE_ROUTES_H
#define HPP_GUARD_MEIOS_XACRO_INCLUDE_ROUTES_H

#include "expand_context.h"
#include "include_locate.h"

#include <pugixml.hpp>

#include <string>
#include <filesystem>

namespace meios::detail
{

bool enter_include(expand_ctx &ctx, pugi::xml_node in, located_include found, pugi::xml_node out);

std::filesystem::path beside_anchor(const expand_ctx &ctx, const std::string &package, const std::string &relative);

bool include_from_sources(expand_ctx &ctx, pugi::xml_node in, const std::string &package, const std::string &normalized, const std::string &written, pugi::xml_node out);

bool include_beside(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &anchor, const std::string &normalized, const std::string &written, pugi::xml_node out);

bool include_climbing(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &anchor, const std::string &written, pugi::xml_node out);

}

#endif

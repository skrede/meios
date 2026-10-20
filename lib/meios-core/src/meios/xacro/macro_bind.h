#ifndef HPP_GUARD_MEIOS_XACRO_MACRO_BIND_H
#define HPP_GUARD_MEIOS_XACRO_MACRO_BIND_H

#include "expand_context.h"

#include <pugixml.hpp>

#include <map>
#include <string>
#include <vector>
#include <filesystem>

namespace meios::detail
{

// One value per declared parameter, in declaration order, and the callee's block map.
struct call_binding
{
    std::vector<value> values;
    std::map<std::string, block_arg> blocks;
};

// Runs in the caller's scope before the macro's frame and origin exist, so a refusal names the
// call's own line and leaves the caller's parameters and blocks as they were.
bool bind_call(expand_ctx &ctx, const macro_def &def, pugi::xml_node call, const std::filesystem::path &document, call_binding &bound);

}

#endif

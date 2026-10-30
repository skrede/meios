#ifndef HPP_GUARD_MEIOS_XACRO_MACRO_BIND_H
#define HPP_GUARD_MEIOS_XACRO_MACRO_BIND_H

#include "expand_context.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <filesystem>

namespace meios::detail
{

// A block argument as the call's table binds it: under its parameter's starred spelling.
struct bound_block
{
    std::string key;
    std::size_t block;
};

// One value per declared parameter, in declaration order, and one block per block argument.
struct call_binding
{
    std::vector<value> values;
    std::vector<bound_block> blocks;
};

// Runs in the caller's scope before the macro's frame and origin exist, so a refusal names the
// call's own line and leaves the caller's tables as they were.
bool bind_call(expand_ctx &ctx, const macro_def &def, pugi::xml_node call, const std::filesystem::path &document, call_binding &bound);

}

#endif

#ifndef HPP_GUARD_MEIOS_XACRO_XACRO_ATTRIBUTES_H
#define HPP_GUARD_MEIOS_XACRO_XACRO_ATTRIBUTES_H

#include "dispatch_view.h"
#include "expand_context.h"

#include <span>
#include <string_view>

namespace meios::detail
{

// The lists are comma-separated, as the derived construct record spells them, so a test can hold
// every row to that record. An element that does not warn reads its other attributes as parameters
// or content.
struct attribute_row
{
    std::string_view tag;
    std::string_view required;
    std::string_view optional;
    bool warns;
};

std::span<const attribute_row> attribute_rows();

// Refuses the first required attribute that is absent, in list order; an attribute present but
// empty is not absent. Otherwise warns once per attribute the row does not list, in document
// order, apart from an xmlns: declaration. An attribute a rename consumed is neither.
bool check_attributes(expand_ctx &ctx, const dispatch_view &view);

}

#endif

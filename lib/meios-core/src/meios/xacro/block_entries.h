#ifndef HPP_GUARD_MEIOS_XACRO_BLOCK_ENTRIES_H
#define HPP_GUARD_MEIOS_XACRO_BLOCK_ENTRIES_H

#include "macro_tables.h"

#include <pugixml.hpp>

namespace meios::detail
{

// What a block entry names: the element a call's argument expanded to, in a document the expansion
// owns, and the origin it was bound under. Insertion walks it again, the element itself or only its
// children, as `*` and `**` select.
struct parked_block
{
    pugi::xml_node node;
    bool content_only;
    emit_origin origin;
};

}

#endif

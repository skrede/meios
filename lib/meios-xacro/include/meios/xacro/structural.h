#ifndef HPP_GUARD_MEIOS_XACRO_STRUCTURAL_H
#define HPP_GUARD_MEIOS_XACRO_STRUCTURAL_H

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <filesystem>
#include <string_view>

namespace meios
{

class source_stack;

struct expansion
{
    bool ok;
    std::string document;
};

// Expands a xacro document into a flat URDF string using only pugixml: recursive
// xacro:include (resolved through sources), xacro:property, xacro:macro with
// params and defaults, *block/**block insertion, and xacro:if/xacro:unless.
// Bounded by limits and an include cycle guard; a failure loud-logs and yields
// ok == false. The document path seeds $(dirname) and diagnostics.
expansion expand(std::string_view source, eval_scope &scope, source_stack &sources,
                 const std::filesystem::path &document, const expansion_limits &limits,
                 log_sink &log);

// Reserializes XML into a canonical form (sorted attributes, collapsed
// insignificant whitespace) so a golden comparison ignores trivial formatting.
std::string canonical_xml(std::string_view xml);

}

#endif

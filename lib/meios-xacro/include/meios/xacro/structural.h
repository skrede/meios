#ifndef HPP_GUARD_MEIOS_XACRO_STRUCTURAL_H
#define HPP_GUARD_MEIOS_XACRO_STRUCTURAL_H

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/eval_policy.h"

#include "meios/diagnostic/log_sink.h"

#include <memory>
#include <string>
#include <filesystem>
#include <string_view>

namespace meios
{

class source_stack;
class evaluator_handle;

struct expansion
{
    bool ok;
    std::string document;
};

// Expands a xacro document into a flat URDF string using only pugixml: recursive
// xacro:include (resolved through sources), xacro:property, xacro:macro with
// params and defaults, *block/**block insertion, and xacro:if/xacro:unless.
// Bounded by limits and an include cycle guard; a failure loud-logs and yields
// ok == false. The document path seeds $(dirname) and diagnostics. eval_policy and
// an optional injected backend govern how an unsupported ${}/$(eval) construct is
// resolved; the delegating overload uses fail policy and the core evaluator.
expansion expand(std::string_view source, eval_scope &scope, source_stack &sources,
                 const std::filesystem::path &document, const expansion_limits &limits,
                 eval_policy policy, const std::shared_ptr<evaluator_handle> &backend,
                 log_sink &log);

expansion expand(std::string_view source, eval_scope &scope, source_stack &sources,
                 const std::filesystem::path &document, const expansion_limits &limits,
                 log_sink &log);

// Reserializes XML into a canonical form (sorted attributes, collapsed
// insignificant whitespace) so a golden comparison ignores trivial formatting.
std::string canonical_xml(std::string_view xml);

namespace detail
{

// Parses a raw argument or property string into a numeric binding when it reads as
// an integer or a real, otherwise keeps it as a string.
binding classify(std::string_view text);

}

}

#endif

#ifndef HPP_GUARD_MEIOS_XACRO_STRUCTURAL_H
#define HPP_GUARD_MEIOS_XACRO_STRUCTURAL_H

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/eval_policy.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/expansion_error.h"

#include "meios/expected.h"

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
    std::string document;
};

// Expands a xacro document into a flat URDF string using only pugixml: recursive
// xacro:include (resolved through sources), xacro:property, xacro:macro with
// params and defaults, *block/**block insertion, and xacro:if/xacro:unless.
// Bounded by limits and an include cycle guard. The document path seeds $(dirname)
// and diagnostics. eval_policy and an optional injected backend govern how an
// unsupported ${}/$(eval) construct is resolved; the delegating overload uses fail
// policy and the core evaluator. A terminal failure is reported once through the
// supplied sink and returned on the error arm; a successful result carries the
// expanded document and nothing else.
expected<expansion, expansion_error> expand(std::string_view source, eval_scope &scope,
                                            source_stack &sources,
                                            const std::filesystem::path &document,
                                            const expansion_limits &limits, eval_policy policy,
                                            const std::shared_ptr<evaluator_handle> &backend,
                                            log_sink &log);

expected<expansion, expansion_error> expand(std::string_view source, eval_scope &scope,
                                            source_stack &sources,
                                            const std::filesystem::path &document,
                                            const expansion_limits &limits, log_sink &log);

// Reserializes XML into a canonical form (sorted attributes, collapsed
// insignificant whitespace) so a golden comparison ignores trivial formatting.
std::string canonical_xml(std::string_view xml);

namespace detail
{

// Parses a raw argument or property string into a numeric value when it reads as an
// integer or a real, otherwise keeps it as a string.
value classify(std::string_view text);

// Only the evaluator may mint a container marker, so a marker present in text an author wrote is a
// forgery and is erased before that text can become a bound value. Apply this to raw authored text
// only, never to a substitution result: that result legitimately carries the markers the evaluator
// minted, and erasing them there would break the container round-trip.
std::string strip_authored_markers(std::string text);

}

}

#endif

#ifndef HPP_GUARD_MEIOS_XACRO_STRUCTURAL_H
#define HPP_GUARD_MEIOS_XACRO_STRUCTURAL_H

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/eval_policy.h"
#include "meios/xacro/include_options.h"
#include "meios/xacro/evaluator_constructs.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/expansion_error.h"

#include "meios/expected.h"

#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <string_view>

namespace meios
{

class source_stack;
class evaluator_handle;

struct expansion
{
    std::string document;
    // Which constructs the load exercised, marked where each was exercised rather than declared
    // anywhere; a load that exercised none leaves it empty.
    construct_set exercised;
};

// Expands a xacro document into a flat URDF string using only pugixml: recursive
// xacro:include, xacro:property, xacro:macro with params and defaults, *block/**block
// insertion, and xacro:if/xacro:unless, bounded by limits and an include cycle guard. A
// relative xacro:include resolves beside the document that writes it (inside a macro body,
// the document that calls the macro), then through sources; a package-qualified or absolute
// one resolves through sources alone. document is the path they resolve beside, as given: a
// relative path anchors at the working directory and an empty one looks for nothing beside
// it; it also seeds $(dirname) and diagnostics. includes lifts the boundaries its members
// name, a climb above the document's directory and a link beside it that leads out of it;
// left at its default, both refuse. package_roots are the directories, beside the document's
// own two, where a file beside the document may lie, as load_options::package_roots are for
// a load; none by default. eval_policy and an optional injected backend govern how an
// unsupported ${}/$(eval) construct is resolved; the delegating overload uses fail policy,
// the core evaluator, the default includes and no package roots. A terminal failure is
// reported once through the supplied sink and returned on the error arm; on the value arm
// the flattened text arrives beside the observation of what this one load exercised.
expected<expansion, expansion_error> expand(std::string_view source, eval_scope &scope,
                                            source_stack &sources,
                                            const std::filesystem::path &document,
                                            const expansion_limits &limits, eval_policy policy,
                                            const std::shared_ptr<evaluator_handle> &backend,
                                            log_sink &log,
                                            const include_options &includes = include_options(),
                                            const std::vector<std::filesystem::path> &package_roots = {});

expected<expansion, expansion_error> expand(std::string_view source, eval_scope &scope,
                                            source_stack &sources,
                                            const std::filesystem::path &document,
                                            const expansion_limits &limits, log_sink &log);

// Reserializes XML into a canonical form (sorted attributes, collapsed
// insignificant whitespace) so a golden comparison ignores trivial formatting.
std::string canonical_xml(std::string_view xml);

namespace detail
{

// Parses a raw property or macro-parameter string into a numeric or boolean value when it
// reads as one, otherwise keeps it as a string. An argument does not come through here: it is
// bound in the substitution domain, and the reading site classifies it.
value classify(std::string_view text);

// Only the evaluator may mint a container marker, so a marker present in text an author wrote is a
// forgery and is erased before that text can become a bound value. Apply this to raw authored text
// only, never to a substitution result: that result legitimately carries the markers the evaluator
// minted, and erasing them there would break the container round-trip.
std::string strip_authored_markers(std::string text);

}

}

#endif

#ifndef HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_H
#define HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_H

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/eval_policy.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"

#include "meios/expected.h"

#include <memory>
#include <string>
#include <filesystem>
#include <string_view>

namespace meios
{

class source_stack;
class evaluator_handle;

struct substitution
{
    std::string text;
};

// Resolves every ${} expression and $() command span in raw against the scope and the
// package sources; the document path seeds $(dirname) and diagnostics. eval_policy and
// an optional injected backend govern how an unsupported construct is resolved; the
// delegating overloads supply the document's own anchor, then fail policy and the core
// evaluator. A terminal failure is reported once through the supplied sink and returned
// on the error arm; a successful result carries the substituted text and nothing else.
// A span the active policy declines to resolve is left verbatim on the value arm rather
// than becoming an error.
expected<substitution, expansion_error> substitute(std::string_view raw, const eval_scope &scope,
                                                   source_stack &sources,
                                                   const std::filesystem::path &document,
                                                   eval_policy policy,
                                                   const std::shared_ptr<evaluator_handle> &backend,
                                                   log_sink &log, const source_location &at);

expected<substitution, expansion_error> substitute(std::string_view raw, const eval_scope &scope,
                                                   source_stack &sources,
                                                   const std::filesystem::path &document,
                                                   eval_policy policy,
                                                   const std::shared_ptr<evaluator_handle> &backend,
                                                   log_sink &log);

expected<substitution, expansion_error> substitute(std::string_view raw, const eval_scope &scope,
                                                   source_stack &sources,
                                                   const std::filesystem::path &document,
                                                   log_sink &log);

}

#endif

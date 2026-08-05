#ifndef HPP_GUARD_MEIOS_XACRO_EXPAND_CONTEXT_H
#define HPP_GUARD_MEIOS_XACRO_EXPAND_CONTEXT_H

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/eval_policy.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"

#include <pugixml.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{
class source_stack;
class evaluator_handle;
}

namespace meios::detail
{

struct emit_origin
{
    std::filesystem::path path;
    std::string_view text;
};

struct macro_def
{
    std::vector<std::string> params;
    std::vector<std::optional<std::string>> defaults;
    std::vector<std::string> block_params;
    std::vector<bool> block_children;
    pugi::xml_node body;
    emit_origin origin;
};

struct block_arg
{
    bool children;
    pugi::xml_node source;
    emit_origin origin;
};

// A macro parameter's outer binding, captured on entry and restored on exit.
using saved_binding = std::pair<std::string, std::optional<binding>>;

// One macro invocation's record of prior property bindings, reverted when the
// invocation exits so a scoped write does not leak past its owning frame.
using prop_frame = std::vector<std::pair<std::string, std::optional<binding>>>;

// Threads the whole expansion: name scope, package sources, both budget counters,
// the macro table, the include cycle stack, the active block bindings, and the stack
// of per-invocation property frames. Parsed include documents are parked in owned so
// macro bodies stay live across files.
struct expand_ctx
{
    expand_ctx(eval_scope &s, source_stack &src, const expansion_limits &lim, eval_policy policy,
               const std::shared_ptr<evaluator_handle> &inject, log_sink &lg);

    eval_scope &scope;
    source_stack &sources;
    const expansion_limits &limits;
    log_sink &log;
    eval_policy mode;
    std::shared_ptr<evaluator_handle> backend;
    expansion_counters counters;
    std::map<std::string, macro_def> macros;
    std::map<std::string, block_arg> blocks;
    std::vector<std::filesystem::path> include_stack;
    std::vector<std::unique_ptr<pugi::xml_document>> owned;
    std::vector<std::unique_ptr<std::string>> owned_text;
    std::vector<emit_origin> origins;
    std::vector<prop_frame> prop_frames;
    // Parallel to prop_frames: the outer bindings each live invocation masked
    // with its parameters, so a scope="parent" write can record the value the
    // ancestor frame actually holds rather than the writing macro's private param.
    std::vector<const std::vector<saved_binding> *> param_saves;
    // The first terminal failure, kept so the walk reports one structured cause; a later
    // failure neither replaces it nor emits a second error-level diagnostic.
    std::optional<expansion_error> terminal;
    bool ok;

    bool charge_work(pugi::xml_node in);
    bool charge_output(pugi::xml_node in);
    pugi::xml_document &park();
};

bool fail(expand_ctx &ctx, const source_location &loc, diagnostic_code code,
          const std::string &message);
bool fail(expand_ctx &ctx, pugi::xml_node in, diagnostic_code code, const std::string &message);
bool fail(expand_ctx &ctx, const source_location &loc, diagnostic_code code,
          const operation_failure &cause, const std::string &message);

void record_terminal(expand_ctx &ctx, const source_location &loc, diagnostic_code code,
                     const std::string &message);

source_location locate(const expand_ctx &ctx, pugi::xml_node in);

}

#endif

#ifndef HPP_GUARD_MEIOS_XACRO_EXPAND_CONTEXT_H
#define HPP_GUARD_MEIOS_XACRO_EXPAND_CONTEXT_H

#include "eval_session.h"
#include "macro_tables.h"
#include "block_entries.h"
#include "deferred_read.h"
#include "include_locate.h"

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/eval_policy.h"
#include "meios/xacro/include_options.h"
#include "meios/xacro/evaluator_limits.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"

#include <pugixml.hpp>

#include <set>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>

namespace meios
{
class source_stack;
class evaluator_handle;
}

namespace meios::detail
{

// Forwards every record unchanged and keeps the first coded, located error as a
// structured cause. A layer beneath expansion names the specific failure — an undefined
// name, an unresolvable package, a rejected asset — while the walk above it knows only
// the structural site it stopped at, so the first such record is the cause worth
// returning. A message-only error carries neither code nor position and is not one.
class terminal_latch final : public log_sink
{
public:
    using log_sink::log;

    explicit terminal_latch(log_sink &inner);

    void log(level lvl, const std::string &message) override;
    void log(level lvl, const source_location &where, const std::string &message) override;
    void log(level lvl, diagnostic_code code, const source_location &where,
             const std::string &message) override;
    void log(level lvl, diagnostic_code code, const source_location &where,
             const operation_failure &cause, const std::string &message) override;

    const std::optional<expansion_error> &first() const { return m_first; }

private:
    log_sink &m_inner;
    std::optional<expansion_error> m_first;

    void latch(level lvl, const source_location &where, diagnostic_code code,
               const std::string &message, std::optional<operation_failure> cause);
};

// Threads the whole expansion: name scope, package sources, both budget counters,
// the macro tables, the include cycle stack and the blocks the calls bound. Parsed include
// documents are parked in owned so macro bodies stay live across files.
struct expand_ctx
{
    expand_ctx(eval_scope &s, source_stack &src, const expansion_limits &lim, const include_options &inc,
               const std::vector<std::filesystem::path> &roots, eval_policy policy,
               const std::shared_ptr<evaluator_handle> &inject, log_sink &lg);

    eval_scope &scope;
    // The argument names the pre-pass lent and no declaration the walk reached has bound since.
    std::set<std::string, std::less<>> seeded_args;
    source_stack &sources;
    const expansion_limits &limits;
    const include_options &includes;
    const std::vector<std::filesystem::path> &package_roots;
    // One load, one set of evaluator ceilings and counters: nothing a caller may share
    // between two loads reaches them.
    evaluator_limits eval_limits;
    eval_session session;
    terminal_latch observer;
    log_sink &log;
    eval_policy mode;
    std::shared_ptr<evaluator_handle> backend;
    expansion_counters counters;
    // How many recursive entries into the walk are live; the counter beside it keeps the
    // high-water mark. It is carried here rather than passed down because the walk's arms
    // are mutually recursive across four files, and a depth threaded through their
    // parameters is one any of them may forget to pass on.
    std::size_t depth;
    macro_chain macros;
    // The blocks every call bound; a block entry indexes this list.
    std::vector<parked_block> blocks;
    std::vector<std::filesystem::path> include_stack;
    // The layer that served the active document, if one did, and the request it answered.
    std::optional<served_document> served;
    std::vector<std::unique_ptr<pugi::xml_document>> owned;
    std::vector<std::unique_ptr<std::string>> owned_text;
    std::vector<emit_origin> origins;
    // Where the block being walked again was inserted: a node the call's first walk produced has no
    // offset of its own, so a refusal on it is located here.
    std::optional<source_location> insert_site;
    // Where each deferred text the scope holds was written; a deferred entry indexes this list.
    std::vector<deferred_origin> deferred;
    // The first terminal failure, kept so the walk reports one structured cause; a later
    // failure neither replaces it nor emits a second error-level diagnostic.
    std::optional<expansion_error> terminal;
    bool ok;

    bool charge_work(pugi::xml_node in);
    bool charge_output(pugi::xml_node in);
    bool enter_node(pugi::xml_node in);
    void leave_node();
    pugi::xml_document &park();
};

// Charges one recursive entry into the walk for as long as it lives. Every arm of the walk
// answers bool on several paths, so the matching leave is tied to a scope rather than written
// out at each of them, where one would eventually be missed and the depth would never unwind.
class depth_guard
{
public:
    depth_guard(expand_ctx &ctx, pugi::xml_node in);
    depth_guard(const depth_guard &) = delete;
    depth_guard &operator=(const depth_guard &) = delete;
    ~depth_guard();

    bool admitted() const { return m_admitted; }

private:
    expand_ctx &m_ctx;
    bool m_admitted;
};

bool fail(expand_ctx &ctx, const source_location &loc, diagnostic_code code,
          const std::string &message);
bool fail(expand_ctx &ctx, pugi::xml_node in, diagnostic_code code, const std::string &message);
bool fail(expand_ctx &ctx, const source_location &loc, diagnostic_code code,
          const operation_failure &cause, const std::string &message);

void record_terminal(expand_ctx &ctx, const source_location &loc, diagnostic_code code,
                     const std::string &message);

// A cause a lower contract already composed — a substitution's own terminal — reaches
// the walk's record whole, so the two report the same code and the same location.
void record_terminal(expand_ctx &ctx, const expansion_error &cause);

// The cause a terminal failure reports: the first coded error any layer emitted, falling
// back to the site the walk recorded and, failing both, to the document itself.
expansion_error terminal_of(const expand_ctx &ctx, const std::filesystem::path &document);

source_location locate(const expand_ctx &ctx, pugi::xml_node in);

}

#endif

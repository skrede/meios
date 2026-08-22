#ifndef HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_DETAIL_H
#define HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_DETAIL_H

#include "eval_session.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/eval_policy.h"
#include "meios/xacro/substitution.h"
#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/evaluator_handle.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include "meios/expected.h"

#include <pugixml.hpp>

#include <cctype>
#include <memory>
#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

class source_stack;
class evaluator_handle;

namespace detail
{

inline std::string_view trim(std::string_view text)
{
    std::size_t begin = text.find_first_not_of(" \t");
    if(begin == std::string_view::npos)
        return {};
    return text.substr(begin, text.find_last_not_of(" \t") - begin + 1);
}

inline bool is_identifier(std::string_view text)
{
    if(text.empty() || (!std::isalpha(static_cast<unsigned char>(text[0])) && text[0] != '_'))
        return false;
    for(char c : text)
        if(!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            return false;
    return true;
}

// The always-on backend: it renders an expression through the core evaluator and,
// on a failure, reports whether the core simply lacks the feature (unsupported) or
// hit a genuine fault (error). eval-python provides a text_evaluator of the same
// shape, so the injection seam and the default path are one interface.
class core_text_evaluator
{
public:
    explicit core_text_evaluator(eval_session &load) : m_session(load) {}

    text_outcome eval_to_text(std::string_view expr, const eval_scope &scope, log_sink &log,
                              const source_location &at = {});

private:
    eval_session &m_session;
};

struct subst_ctx
{
    subst_ctx(log_sink &sink, source_stack &pkg_sources, const eval_scope &names,
              const std::filesystem::path &doc, eval_policy policy,
              const std::shared_ptr<evaluator_handle> &inject, eval_session &load,
              const source_location &anchor = {})
        : log(sink), sources(pkg_sources), scope(names), session(load), core(load), document(doc),
          at(anchor), node_anchor(anchor), mode(policy), backend(inject),
          last_kind(eval_failure_kind::none), terminal()
    {
    }

    log_sink &log;
    source_stack &sources;
    const eval_scope &scope;
    eval_session &session;
    core_text_evaluator core;
    const std::filesystem::path &document;
    source_location at;
    // The immutable node-anchored floor: `at`'s column may be refined to a failing
    // token, but a degrade always falls back to this, never to a prior span's column.
    source_location node_anchor;
    eval_policy mode;
    std::shared_ptr<evaluator_handle> backend;
    eval_failure_kind last_kind;
    // The first terminal failure, kept so one refusal reports one structured cause; a
    // later refusal neither replaces it nor emits a second error-level diagnostic.
    std::optional<expansion_error> terminal;
    // Optional forward-scan refinement inputs, empty for the public substitute() path:
    // the hosting element, its raw document text, and the attribute's DOM index.
    pugi::xml_node host{};
    std::string_view host_text{};
    std::optional<std::size_t> attr_index{};
};

inline void record_terminal(subst_ctx &ctx, const expansion_error &cause)
{
    if(!ctx.terminal)
        ctx.terminal = cause;
}

inline void record_terminal(subst_ctx &ctx, diagnostic_code code, const std::string &message)
{
    record_terminal(ctx, expansion_error{ ctx.at, message, code, std::nullopt });
}

// The expression-depth axis counts stack frames in units of one recursive entry into the
// expression grammar, so a level costing more than one entry is charged what it costs. Measured
// on a 512 KiB thread by driving each shape until the process died: 2 424 grammar entries, 870
// nested scans and 228 nested substitutions survive, where 2 448, 880 and 229 do not. A nested
// scan re-enters the scanner alone; a nested substitution builds a context of its own and runs
// the whole dispatch beneath it. So charged, the one ceiling leaves each shape ninefold headroom.
inline constexpr std::size_t nested_scan_cost = 3;
inline constexpr std::size_t nested_substitution_cost = 11;

// One level of span nesting, released by the same unwind that abandons it. A crossed ceiling is
// reported by the session against the real sink, so its cause is copied here for the caller.
struct span_level
{
    span_level(subst_ctx &owner, std::size_t weight)
        : ctx(owner), cost(weight), entered(owner.session.enter_span(weight, owner.log, owner.at))
    {
        if(!entered && ctx.session.terminal)
            record_terminal(ctx, *ctx.session.terminal);
    }

    span_level(const span_level &) = delete;

    ~span_level() { if(entered) ctx.session.leave_span(cost); }

    bool admitted() const { return entered; }

private:
    subst_ctx &ctx;
    std::size_t cost;
    bool entered;
};

std::optional<std::string> eval_expr(subst_ctx &ctx, std::string_view expression);

std::optional<std::string> dispatch(subst_ctx &ctx, std::string_view inner);

// An exact substitution keeps the evaluated value's type; mixed text renders it. The two
// paths must agree on what "exact" means, so this predicate is the single place that
// decides it: the trimmed text is one ${...} span, closed where the scanner closes it.
std::optional<std::string_view> exact_expression(std::string_view raw);

// Answers with the value an exact span evaluates to, or nothing when the span failed or a
// lenient policy retained it verbatim; ctx.last_kind distinguishes the two.
std::optional<value> eval_exact(subst_ctx &ctx, std::string_view expr);

// The error arm is a terminal failure; an empty value arm is a span a lenient policy
// retained verbatim, which the caller binds as the text the author wrote.
expected<std::optional<value>, expansion_error> substitute_exact(
    std::string_view expr, const eval_scope &scope, source_stack &sources,
    const std::filesystem::path &document, eval_policy policy,
    const std::shared_ptr<evaluator_handle> &backend, eval_session &session, log_sink &log,
    const source_location &at);

// The scanner and the span machinery are mutually recursive: a command span resolves
// its own inner text before dispatching, so `base` accumulates the enclosing span's
// offset and a nested token still maps back to its real column.
bool scan(subst_ctx &ctx, std::string_view raw, std::string &out, std::size_t base);

bool expand_span(subst_ctx &ctx, std::string_view raw, std::size_t dollar, std::string &out,
                 std::size_t &cursor, std::size_t base);

// Internal substitute that charges the load's own session and carries the forward-scan
// refinement inputs so an attribute-hosted failing token's column can refine the node
// anchor. The public substitute() overloads keep the node anchor, do not widen for this,
// and charge a session of their own that lives exactly as long as the call.
expected<substitution, expansion_error> substitute_refined(
    std::string_view raw, const eval_scope &scope, source_stack &sources,
    const std::filesystem::path &document, eval_policy policy,
    const std::shared_ptr<evaluator_handle> &backend, eval_session &session, log_sink &log,
    const source_location &at, pugi::xml_node host = {}, std::string_view host_text = {},
    std::optional<std::size_t> attr_index = std::nullopt);

// A nested resolution inside an already-running substitution, sharing its session so the
// inner text is charged to the same load rather than to a fresh budget.
expected<substitution, expansion_error> substitute_in(subst_ctx &ctx, std::string_view raw);

}

}

#endif

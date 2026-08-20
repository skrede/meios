#include "quote_scan.h"
#include "substitution_detail.h"

#include "meios/detail/text_location.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

// Upgrades ctx.at's column to the failing token at `decoded_offset` into the attribute
// value, when the refinement inputs are present; line and file stay the node anchor's.
// The forward-scan self-check degrades to the value-start or node-anchor column, so a
// column ctx.at carries is always one the scan confirmed.
void refine_span_column(subst_ctx &ctx, std::size_t decoded_offset)
{
    if(!ctx.attr_index || !ctx.host)
        return;
    ctx.at.column = refine_attr_column(ctx.host_text, ctx.host, *ctx.attr_index, decoded_offset,
                                       ctx.node_anchor).column;
}

std::size_t find_cmd_close(std::string_view raw, std::size_t opener)
{
    int depth = 1;
    for(std::size_t k = opener + 1; k < raw.size(); ++k)
    {
        if(raw[k] == '(')
            ++depth;
        else if(raw[k] == ')' && --depth == 0)
            return k;
    }
    return std::string_view::npos;
}

// A brace inside a string literal is that literal's text and so steers no span. Command
// spans are deliberately excluded from this rule: a command resolves its inner text
// before dispatching and therefore nests, where upstream's command scanner is
// quote-blind.
std::size_t find_expr_close(std::string_view raw, std::size_t opener)
{
    char open = '\0';
    int depth = 1;
    for(std::size_t k = opener + 1; k < raw.size(); ++k)
    {
        open = step_quote(open, raw[k]);
        if(open != '\0')
            continue;
        if(raw[k] == '{')
            ++depth;
        else if(raw[k] == '}' && --depth == 0)
            return k;
    }
    return std::string_view::npos;
}

bool fail_unterminated(subst_ctx &ctx, char opener, std::size_t at)
{
    const std::string message = std::string("unterminated $") + opener
                              + " substitution span at offset " + std::to_string(at);
    ctx.log.log(level::error, diagnostic_code::unterminated_substitution, ctx.at, message);
    record_terminal(ctx, diagnostic_code::unterminated_substitution, message);
    return false;
}

// Under warn/skip a construct the core cannot evaluate is left verbatim in the
// output — never fabricated, never blanked — so the resolved text visibly carries
// the unevaluated expression. A genuine error still fails; a conditional is resolved
// with fail policy elsewhere, so leniency reaches text/attribute spans only.
bool leave_verbatim(subst_ctx &ctx, std::string_view raw, std::size_t dollar, std::size_t close,
                    std::string &out, std::size_t &cursor)
{
    if(ctx.last_kind != eval_failure_kind::unsupported
       || (ctx.mode != eval_policy::warn && ctx.mode != eval_policy::skip))
        return false;
    if(ctx.mode == eval_policy::warn)
        ctx.log.log(level::warn, diagnostic_code::unsupported_expression, ctx.at,
            "left an unsupported substitution verbatim at offset " + std::to_string(dollar)
                + " in " + ctx.document.string());
    out.append(raw.substr(dollar, close - dollar + 1));
    cursor = close + 1;
    return true;
}

std::string_view leading_command(std::string_view inner)
{
    std::size_t start = inner.find_first_not_of(" \t");
    if(start == std::string_view::npos)
        return {};
    std::size_t end = inner.find_first_of(" \t", start);
    return inner.substr(start, end - start);
}

// The ceiling this level answers to is the one the expression grammar's own descent answers to.
bool scan_nested(subst_ctx &ctx, std::string_view inner, std::string &out, std::size_t base)
{
    const span_level level(ctx, nested_scan_cost);
    return level.admitted() && scan(ctx, inner, out, base);
}

// Upstream resolves a substitution as safe_eval(eval_text(body)): a span's inner text is
// expanded first and the string that produces is what is then evaluated or dispatched, so a
// package-locating command written inside a quoted literal has already become a path by the
// time the expression runs. Both openers therefore take the same inner scan. `$(arg name
// default)` is the exception: its inner is dispatched unscanned so the `default` is evaluated
// lazily by `cmd_arg` only on the unset branch — a pre-scan would eagerly evaluate a default
// the bound arg never uses, failing on an unresolvable fallback. A failed inner scan yields
// nullopt so the caller's verbatim/leniency handling governs the outcome rather than a
// mis-dispatch. `span_offset` is the failing span's `$` byte as an offset into the whole decoded
// attribute value; nested scans accumulate it so a token in `$(find ${x})` still maps back to
// the real column. After a nested scan clobbers ctx.at, the outer span is re-anchored to its own
// offset before it can emit.
std::optional<std::string> resolve_span(subst_ctx &ctx, char opener, std::string_view inner,
                                        std::size_t span_offset)
{
    if(opener == '(' && leading_command(inner) == "arg")
    {
        ctx.last_kind = eval_failure_kind::none;
        return dispatch(ctx, inner);
    }
    std::string resolved;
    if(!scan_nested(ctx, inner, resolved, span_offset + 2))
        return std::nullopt;
    refine_span_column(ctx, span_offset);
    ctx.last_kind = eval_failure_kind::none;
    return opener == '{' ? eval_expr(ctx, resolved) : dispatch(ctx, resolved);
}

}

bool expand_span(subst_ctx &ctx, std::string_view raw, std::size_t dollar, std::string &out,
                 std::size_t &cursor, std::size_t base)
{
    const std::size_t span_offset = base + dollar;
    refine_span_column(ctx, span_offset);
    char opener = raw[dollar + 1];
    std::size_t close = opener == '{' ? find_expr_close(raw, dollar + 1)
                                      : find_cmd_close(raw, dollar + 1);
    if(close == std::string_view::npos)
        return fail_unterminated(ctx, opener, dollar);
    std::string_view inner = raw.substr(dollar + 2, close - dollar - 2);
    std::optional<std::string> result = resolve_span(ctx, opener, inner, span_offset);
    if(result)
    {
        out.append(*result);
        cursor = close + 1;
        return true;
    }
    return leave_verbatim(ctx, raw, dollar, close, out, cursor);
}

}

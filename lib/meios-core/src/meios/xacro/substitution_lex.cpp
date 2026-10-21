#include "substitution_lex.h"
#include "substitution_detail.h"

#include "meios/detail/text_location.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <cstddef>
#include <string_view>

namespace meios::detail
{

namespace
{

bool refuse_dollar_run(subst_ctx &ctx, std::string_view raw, std::size_t dollar, std::size_t base)
{
    refine_span_column(ctx, base + dollar);
    const std::string message = "invalid expression: " + std::string(raw.substr(dollar));
    ctx.log.log(level::error, diagnostic_code::expression_error, ctx.at, message);
    record_terminal(ctx, diagnostic_code::expression_error, message);
    // Upstream's lexer refuses under every policy, so no enclosing span may keep this verbatim.
    ctx.last_kind = eval_failure_kind::error;
    return false;
}

}

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

bool fail_unterminated(subst_ctx &ctx, char opener, std::size_t at)
{
    const std::string message = std::string("unterminated $") + opener
                              + " substitution span at offset " + std::to_string(at);
    ctx.log.log(level::error, diagnostic_code::unterminated_substitution, ctx.at, message);
    record_terminal(ctx, diagnostic_code::unterminated_substitution, message);
    // An unterminated span refuses under every policy, so no enclosing span may keep it verbatim.
    ctx.last_kind = eval_failure_kind::error;
    return false;
}

bool refuse_follower(subst_ctx &ctx, std::string_view raw, std::size_t at, std::size_t base)
{
    if(raw[at + 1] == '$')
        return refuse_dollar_run(ctx, raw, at, base);
    refine_span_column(ctx, base + at);
    return fail_unterminated(ctx, raw[at + 1], at);
}

bool expand_dollar_run(subst_ctx &ctx, std::string_view raw, std::size_t dollar, std::string &out,
                       std::size_t &cursor, std::size_t base)
{
    const std::size_t end = raw.find_first_not_of('$', dollar);
    if(end != std::string_view::npos && (raw[end] == '{' || raw[end] == '('))
    {
        out.append(end - dollar - 1, '$');
        out.push_back(raw[end]);
        cursor = end + 1;
        return true;
    }
    return refuse_dollar_run(ctx, raw, dollar, base);
}

}

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

// A refusal holds under every policy, so marking the kind here keeps any enclosing span from
// leaving it verbatim.
bool refuse_terminal(subst_ctx &ctx, diagnostic_code code, const std::string &message)
{
    ctx.log.log(level::error, code, ctx.at, message);
    record_terminal(ctx, code, message);
    ctx.last_kind = eval_failure_kind::error;
    return false;
}

bool refuse_recorded(subst_ctx &ctx, const expansion_error &cause)
{
    record_terminal(ctx, cause);
    ctx.last_kind = eval_failure_kind::error;
    return false;
}

namespace
{

bool refuse_dollar_run(subst_ctx &ctx, std::string_view raw, std::size_t dollar, std::size_t base)
{
    refine_span_column(ctx, base + dollar);
    return refuse_terminal(ctx, diagnostic_code::expression_error, "invalid expression: " + std::string(raw.substr(dollar)));
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
    return refuse_terminal(ctx, diagnostic_code::unterminated_substitution,
                           std::string("unterminated $") + opener + " substitution span at offset " + std::to_string(at));
}

bool refuse_follower(subst_ctx &ctx, std::string_view raw, std::size_t at, std::size_t base)
{
    if(raw[at + 1] == '$')
        return refuse_dollar_run(ctx, raw, at, base);
    refine_span_column(ctx, base + at);
    return fail_unterminated(ctx, raw[at + 1], base + at);
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

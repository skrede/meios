#include "substitution_detail.h"

#include "meios/xacro/substitution.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace
{

std::size_t find_close(std::string_view raw, std::size_t opener, char open_ch, char close_ch)
{
    int depth = 1;
    for(std::size_t k = opener + 1; k < raw.size(); ++k)
    {
        if(raw[k] == open_ch)
            ++depth;
        else if(raw[k] == close_ch && --depth == 0)
            return k;
    }
    return std::string_view::npos;
}

bool fail_unterminated(detail::subst_ctx &ctx, char opener, std::size_t at)
{
    ctx.log.log(level::error, std::string("unterminated $") + opener
        + " substitution span at offset " + std::to_string(at));
    return false;
}

// Under warn/skip a construct the core cannot evaluate is left verbatim in the
// output — never fabricated, never blanked — so the resolved text visibly carries
// the unevaluated expression. A genuine error still fails; a conditional is resolved
// with fail policy elsewhere, so leniency reaches text/attribute spans only.
bool leave_verbatim(detail::subst_ctx &ctx, std::string_view raw, std::size_t dollar,
                    std::size_t close, std::string &out, std::size_t &cursor)
{
    if(ctx.last_kind != eval_failure_kind::unsupported
       || (ctx.mode != eval_policy::warn && ctx.mode != eval_policy::skip))
        return false;
    if(ctx.mode == eval_policy::warn)
        ctx.log.log(level::warn, "left an unsupported substitution verbatim at offset "
            + std::to_string(dollar) + " in " + ctx.document.string());
    out.append(raw.substr(dollar, close - dollar + 1));
    cursor = close + 1;
    return true;
}

bool scan(detail::subst_ctx &ctx, std::string_view raw, std::string &out);

std::string_view leading_command(std::string_view inner)
{
    std::size_t start = inner.find_first_not_of(" \t");
    if(start == std::string_view::npos)
        return {};
    std::size_t end = inner.find_first_of(" \t", start);
    return inner.substr(start, end - start);
}

// A `$(...)` command first resolves any `${}`/`$()` in its inner text, so
// `$(find ${pkg})` dispatches the resolved package name; a `${...}` expression is
// handed to the evaluator whole. `$(arg name default)` is the exception: its inner
// is dispatched unscanned so the `default` is evaluated lazily by `cmd_arg` only on
// the unset branch — a pre-scan would eagerly evaluate a default the bound arg never
// uses, failing on an unresolvable fallback. A failed inner scan yields nullopt so
// the caller's verbatim/leniency handling governs the outcome rather than a mis-dispatch.
std::optional<std::string> resolve_span(detail::subst_ctx &ctx, char opener, std::string_view inner)
{
    if(opener == '{')
    {
        ctx.last_kind = eval_failure_kind::none;
        return detail::eval_expr(ctx, inner);
    }
    if(leading_command(inner) == "arg")
    {
        ctx.last_kind = eval_failure_kind::none;
        return detail::dispatch(ctx, inner);
    }
    std::string resolved;
    if(!scan(ctx, inner, resolved))
        return std::nullopt;
    ctx.last_kind = eval_failure_kind::none;
    return detail::dispatch(ctx, resolved);
}

bool expand_span(detail::subst_ctx &ctx, std::string_view raw, std::size_t dollar,
                 std::string &out, std::size_t &cursor)
{
    char opener = raw[dollar + 1];
    char closer = opener == '{' ? '}' : ')';
    std::size_t close = find_close(raw, dollar + 1, opener, closer);
    if(close == std::string_view::npos)
        return fail_unterminated(ctx, opener, dollar);
    std::string_view inner = raw.substr(dollar + 2, close - dollar - 2);
    std::optional<std::string> result = resolve_span(ctx, opener, inner);
    if(result)
    {
        out.append(*result);
        cursor = close + 1;
        return true;
    }
    return leave_verbatim(ctx, raw, dollar, close, out, cursor);
}

bool scan(detail::subst_ctx &ctx, std::string_view raw, std::string &out)
{
    std::size_t cursor = 0;
    while(cursor < raw.size())
    {
        std::size_t dollar = raw.find('$', cursor);
        if(dollar == std::string_view::npos || dollar + 1 >= raw.size())
            break;
        out.append(raw.substr(cursor, dollar - cursor));
        char opener = raw[dollar + 1];
        if(opener != '{' && opener != '(')
        {
            out.push_back('$');
            cursor = dollar + 1;
            continue;
        }
        if(!expand_span(ctx, raw, dollar, out, cursor))
            return false;
    }
    out.append(raw.substr(cursor));
    return true;
}

}

substitution substitute(std::string_view raw, const eval_scope &scope, source_stack &sources,
                        const std::filesystem::path &document, eval_policy policy,
                        evaluator_handle *backend, log_sink &log)
{
    detail::subst_ctx ctx(log, sources, scope, document, policy, backend);
    std::string out;
    bool ok = scan(ctx, raw, out);
    return substitution{ ok, std::move(out) };
}

substitution substitute(std::string_view raw, const eval_scope &scope, source_stack &sources,
                        const std::filesystem::path &document, log_sink &log)
{
    return substitute(raw, scope, sources, document, eval_policy::fail, nullptr, log);
}

}

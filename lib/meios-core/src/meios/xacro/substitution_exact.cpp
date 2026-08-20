#include "quote_scan.h"
#include "eval_session.h"
#include "substitution_sinks.h"
#include "substitution_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/evaluator_handle.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"

#include "meios/expected.h"

#include <memory>
#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

std::optional<value> run_core(subst_ctx &ctx, std::string_view expr, log_sink &sink)
{
    core_evaluator evaluator;
    value result  = evaluator.eval(expr, ctx.scope, sink, ctx.at, ctx.session);
    ctx.last_kind = evaluator.failed() ? evaluator.failure_kind() : eval_failure_kind::none;
    if(evaluator.failed())
        return std::nullopt;
    return result;
}

// The policy shape eval_expr uses, kept over a value: leniency reaches an unsupported
// construct alone, and anything else is replayed loud and stays terminal.
std::optional<value> eval_native(subst_ctx &ctx, std::string_view expr)
{
    latching_sink loud(ctx);
    if(ctx.mode == eval_policy::fail)
        return run_core(ctx, expr, loud);
    capture_sink buffer;
    std::optional<value> out = run_core(ctx, expr, buffer);
    if(out)
    {
        buffer.replay_warnings(loud);
        return out;
    }
    if(ctx.last_kind != eval_failure_kind::unsupported)
        buffer.replay(loud);
    return std::nullopt;
}

// The inner text resolves before the expression runs, on the same terms and at the same cost
// as a span the scanner reaches: an exact span is one span, and which caller found it must not
// change what it means.
bool scan_inner(subst_ctx &ctx, std::string_view expr, std::string &out)
{
    const span_level level(ctx, nested_scan_cost);
    return level.admitted() && scan(ctx, expr, out, 0);
}

}

std::optional<std::string_view> exact_expression(std::string_view raw)
{
    const std::string_view text = trim(raw);
    if(text.size() < 3 || text.substr(0, 2) != "${")
        return std::nullopt;
    if(find_expr_close(text, 1) != text.size() - 1)
        return std::nullopt;
    return text.substr(2, text.size() - 3);
}

// Every text that crosses this boundary is reclassified, whether it came from a bound name or
// from an expression, so an exact span and mixed text agree on what a numeric-looking or
// boolean-looking text means; a collection crosses intact, having no text to reclassify. The
// reference does the same to whatever a bound value evaluates to. An injected backend keeps the
// text path it has always had, which is what leaves its container transport untouched.
value reclassified(const value &read)
{
    return read.kind() == value_kind::string ? classify(*read.text()) : read;
}

std::optional<value> eval_exact(subst_ctx &ctx, std::string_view expr)
{
    ctx.last_kind = eval_failure_kind::none;
    const std::string_view name = trim(expr);
    if(is_identifier(name))
    {
        std::optional<value> bound = ctx.scope.lookup(name);
        if(bound)
            return reclassified(*bound);
    }
    std::string resolved;
    if(!scan_inner(ctx, expr, resolved))
        return std::nullopt;
    ctx.last_kind = eval_failure_kind::none;
    if(ctx.backend)
    {
        std::optional<std::string> text = eval_expr(ctx, resolved);
        return text ? std::optional<value>(classify(*text)) : std::nullopt;
    }
    std::optional<value> read = eval_native(ctx, resolved);
    if(!read)
        return std::nullopt;
    return reclassified(*read);
}

expected<std::optional<value>, expansion_error> substitute_exact(
    std::string_view expr, const eval_scope &scope, source_stack &sources,
    const std::filesystem::path &document, eval_policy policy,
    const std::shared_ptr<evaluator_handle> &backend, eval_session &session, log_sink &log,
    const source_location &at)
{
    subst_ctx ctx(log, sources, scope, document, policy, backend, session, at);
    std::optional<value> bound = eval_exact(ctx, expr);
    const bool retained = !bound && ctx.last_kind == eval_failure_kind::unsupported
                       && (policy == eval_policy::warn || policy == eval_policy::skip);
    if(retained && policy == eval_policy::warn)
        log.log(level::warn, diagnostic_code::unsupported_expression, at,
                "left an unsupported substitution verbatim: ${" + std::string(expr) + '}');
    if(bound || retained)
        return bound;
    if(ctx.terminal)
        return unexpected<expansion_error>(*ctx.terminal);
    return unexpected<expansion_error>(expansion_error{});
}

}

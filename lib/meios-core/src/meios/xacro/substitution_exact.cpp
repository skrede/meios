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

}

std::optional<std::string_view> exact_expression(std::string_view raw)
{
    const std::string_view text = trim(raw);
    if(text.size() < 3 || text.substr(0, 2) != "${" || text.back() != '}')
        return std::nullopt;
    const std::string_view inner = text.substr(2, text.size() - 3);
    if(inner.find('$') != std::string_view::npos || inner.find('}') != std::string_view::npos)
        return std::nullopt;
    return inner;
}

// A bound name answers with the value it already holds, so a collection crosses the
// boundary intact; a bound string is reclassified so an exact span and mixed text agree on
// what a numeric-looking text means. An injected backend keeps the text path it has
// always had, which is what leaves its container transport untouched.
std::optional<value> eval_exact(subst_ctx &ctx, std::string_view expr)
{
    ctx.last_kind = eval_failure_kind::none;
    const std::string_view name = trim(expr);
    if(is_identifier(name))
    {
        std::optional<value> bound = ctx.scope.lookup(name);
        if(bound)
            return bound->kind() == value_kind::string ? classify(*bound->text()) : *bound;
    }
    if(!ctx.backend)
        return eval_native(ctx, expr);
    std::optional<std::string> text = eval_expr(ctx, expr);
    if(!text)
        return std::nullopt;
    return classify(*text);
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

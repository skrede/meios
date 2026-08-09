#include "eval_session.h"
#include "substitution_sinks.h"
#include "substitution_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/value_render.h"
#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/evaluator_handle.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios
{

namespace detail
{

// A collection has no scalar spelling, so it fails here as an error rather than as
// unsupported: no policy may soften it into a partial document, and the diagnostic names
// the expression and the kind without ever writing the collection's contents.
text_outcome core_text_evaluator::eval_to_text(std::string_view expr, const eval_scope &scope, log_sink &log, const source_location &at)
{
    core_evaluator evaluator;
    value result = evaluator.eval(expr, scope, log, at, m_session);
    if(evaluator.failed())
        return text_outcome{ std::nullopt, evaluator.failure_kind() };
    std::optional<std::string> rendered = render_scalar(result);
    if(rendered)
        return text_outcome{ std::move(rendered), eval_failure_kind::none };
    log.log(level::error, diagnostic_code::expression_error, at,
            "the expression \"" + std::string(expr) + "\" is a "
                + std::string(kind_name(result.kind())) + ", which has no text form");
    return text_outcome{ std::nullopt, eval_failure_kind::error };
}

static_assert(text_evaluator<core_text_evaluator>);

namespace
{

std::optional<std::string> string_property(subst_ctx &ctx, std::string_view name)
{
    if(!is_identifier(name))
        return std::nullopt;
    std::optional<value> bound = ctx.scope.lookup(name);
    if(bound && bound->kind() == value_kind::string)
        return bound->text();
    return std::nullopt;
}

std::optional<std::string> run_backend(subst_ctx &ctx, std::string_view expr, log_sink &sink)
{
    if(ctx.backend)
    {
        relocating_sink relocate(sink, ctx.at);
        text_outcome out = ctx.backend->eval_to_text(expr, ctx.scope, relocate);
        ctx.last_kind    = out.failure;
        return out.text;
    }
    text_outcome out = ctx.core.eval_to_text(expr, ctx.scope, sink, ctx.at);
    ctx.last_kind     = out.failure;
    return out.text;
}

}

// fail runs the backend loud against the real sink. Under warn/skip the backend's
// diagnostics are captured: a genuine error and a refusal are replayed and still
// hard-fail, while an unsupported failure is left to expand_span to leave verbatim.
// Either way the loud path runs through the latch, so a terminal evaluation failure
// carries the code the evaluator itself reported. A warning about a value that did resolve
// survives a lenient policy, unlike a failure diagnostic: the value reached output, so what
// was said about it must reach the author too.
std::optional<std::string> eval_expr(subst_ctx &ctx, std::string_view expression)
{
    std::string_view expr             = trim(expression);
    std::optional<std::string> direct = string_property(ctx, expr);
    if(direct)
        return direct;
    latching_sink loud(ctx);
    if(ctx.mode == eval_policy::fail)
        return run_backend(ctx, expr, loud);
    capture_sink buffer;
    std::optional<std::string> out = run_backend(ctx, expr, buffer);
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

}

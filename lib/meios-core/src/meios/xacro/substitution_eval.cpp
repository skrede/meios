#include "substitution_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/evaluator_handle.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <tuple>
#include <string>
#include <vector>
#include <variant>
#include <utility>
#include <optional>
#include <string_view>

namespace meios
{

namespace detail
{

std::optional<std::string> core_text_evaluator::eval_to_text(std::string_view expr, const eval_scope &scope, log_sink &log, const source_location &at)
{
    core_evaluator evaluator;
    value result = evaluator.eval(expr, scope, log, at);
    if(evaluator.failed())
    {
        m_kind = evaluator.failure_kind();
        return std::nullopt;
    }
    m_kind = eval_failure_kind::none;
    return to_python_str(result);
}

static_assert(text_evaluator<core_text_evaluator>);

namespace
{

// Buffers an evaluator's diagnostics so the policy layer can decide their fate: a
// genuine error is replayed loud, an unsupported-under-leniency failure is dropped.
class capture_sink final : public log_sink
{
public:
    using log_sink::log;

    void log(level lvl, const std::string &message) override
    {
        m_plain.emplace_back(lvl, message);
    }

    void log(level lvl, const source_location &where, const std::string &message) override
    {
        m_located.emplace_back(lvl, where, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &where, const std::string &message) override
    {
        m_coded.emplace_back(lvl, code, where, message);
    }

    void replay(log_sink &sink) const
    {
        for(const std::pair<level, std::string> &entry : m_plain)
            sink.log(entry.first, entry.second);
        for(const std::tuple<level, source_location, std::string> &entry : m_located)
            sink.log(std::get<0>(entry), std::get<1>(entry), std::get<2>(entry));
        for(const std::tuple<level, diagnostic_code, source_location, std::string> &entry : m_coded)
            sink.log(std::get<0>(entry), std::get<1>(entry), std::get<2>(entry), std::get<3>(entry));
    }

private:
    std::vector<std::pair<level, std::string>> m_plain;
    std::vector<std::tuple<level, source_location, std::string>> m_located;
    std::vector<std::tuple<level, diagnostic_code, source_location, std::string>> m_coded;
};

// Upgrades an unlocated, code-less error the injected backend emits to a typed, located
// record anchored at the hosting node, so a diagnostic escaping a message-only evaluator
// still leaves the layer with a code and a real position.
class relocating_sink final : public log_sink
{
public:
    using log_sink::log;

    relocating_sink(log_sink &inner, const source_location &at)
            : m_inner(inner)
            , m_at(at)
    {
    }

    void log(level lvl, const std::string &message) override
    {
        if(lvl == level::error)
            m_inner.log(lvl, diagnostic_code::expression_error, m_at, message);
        else
            m_inner.log(lvl, message);
    }

    void log(level lvl, const source_location &where, const std::string &message) override
    {
        m_inner.log(lvl, where, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &where, const std::string &message) override
    {
        m_inner.log(lvl, code, where, message);
    }

private:
    log_sink &m_inner;
    source_location m_at;
};

std::optional<std::string> string_property(subst_ctx &ctx, std::string_view name)
{
    if(!is_identifier(name))
        return std::nullopt;
    std::optional<binding> bound = ctx.scope.lookup(name);
    if(bound && std::holds_alternative<std::string>(*bound))
        return std::get<std::string>(*bound);
    return std::nullopt;
}

std::optional<std::string> run_backend(subst_ctx &ctx, std::string_view expr, log_sink &sink)
{
    if(ctx.backend)
    {
        relocating_sink relocate(sink, ctx.at);
        std::optional<std::string> out = ctx.backend->eval_to_text(expr, ctx.scope, relocate);
        ctx.last_kind                  = out ? eval_failure_kind::none : ctx.backend->last_failure_kind();
        return out;
    }
    std::optional<std::string> out = ctx.core.eval_to_text(expr, ctx.scope, sink, ctx.at);
    ctx.last_kind                  = out ? eval_failure_kind::none : ctx.core.last_failure_kind();
    return out;
}

}

// fail runs the backend loud against the real sink. Under warn/skip the backend's
// diagnostics are captured: a genuine error and a refusal are replayed and still
// hard-fail, while an unsupported failure is left to expand_span to leave verbatim.
std::optional<std::string> eval_expr(subst_ctx &ctx, std::string_view expression)
{
    std::string_view expr             = trim(expression);
    std::optional<std::string> direct = string_property(ctx, expr);
    if(direct)
        return direct;
    if(ctx.mode == eval_policy::fail)
        return run_backend(ctx, expr, ctx.log);
    capture_sink buffer;
    std::optional<std::string> out = run_backend(ctx, expr, buffer);
    if(out)
        return out;
    if(ctx.last_kind == eval_failure_kind::error || ctx.last_kind == eval_failure_kind::refused)
        buffer.replay(ctx.log);
    return std::nullopt;
}

}

}

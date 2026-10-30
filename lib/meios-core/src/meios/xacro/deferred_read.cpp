#include "scope_chain.h"
#include "eval_session.h"
#include "deferred_read.h"
#include "expand_context.h"
#include "deferred_message.h"
#include "structural_detail.h"
#include "substitution_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/substitution.h"
#include "meios/xacro/eval_failure_kind.h"

#include "meios/xacro/detail/scope_entry.h"
#include "meios/xacro/detail/table_chain.h"

#include "meios/detail/text_location.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"

#include "meios/expected.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

source_location definition_at(const expand_ctx &ctx, std::size_t origin)
{
    const deferred_origin &from = ctx.deferred[origin];
    if(from.host.offset_debug() < 0)
        return source_location{ from.origin.path, 0, 0 };
    return offset_location(from.origin.text, from.host.offset_debug(), from.origin.path);
}

// One resolution's span level, released by the same unwind that abandons it. A crossed ceiling is
// reported into the level's own sink, so the refusal names the property it stopped at.
class resolution_level
{
public:
    resolution_level(expand_ctx &ctx, std::size_t weight, const source_location &at, log_sink &log)
        : m_ctx(ctx), m_weight(weight), m_entered(ctx.session.enter_span(weight, log, at))
    {
    }

    resolution_level(const resolution_level &) = delete;
    resolution_level &operator=(const resolution_level &) = delete;

    ~resolution_level()
    {
        if(m_entered)
            m_ctx.session.leave_span(m_weight);
    }

    bool admitted() const
    {
        return m_entered;
    }

private:
    expand_ctx &m_ctx;
    std::size_t m_weight;
    bool m_entered;
};

std::optional<value> substitute_level(expand_ctx &ctx, const deferred_origin &from, std::string_view text,
                                      log_sink &log)
{
    const source_location at = locate(ctx, from.host);
    const std::optional<std::string_view> inner = exact_expression(text);
    if(!inner)
    {
        const expected<substitution, expansion_error> mixed =
            substitute_refined(text, ctx.scope, ctx.sources, from.document, ctx.mode, ctx.backend, ctx.session, log,
                               at, from.host, from.origin.text, from.attr_index);
        return mixed ? std::optional<value>(classify(mixed->text)) : std::nullopt;
    }
    const expected<std::optional<value>, expansion_error> exact = substitute_exact(
        *inner, ctx.scope, ctx.sources, from.document, ctx.mode, ctx.backend, ctx.session, log, at);
    if(!exact)
        return std::nullopt;
    return *exact ? **exact : classify(text);
}

// A refusal is located inside the definition's own text, but the active document stays the file
// being processed: the reference resolves a relative path when the property is read, not where it
// was written.
name_read evaluate_level(expand_ctx &ctx, const deferred_text &deferred, std::size_t carried, log_sink &log)
{
    const resolution_level level(ctx, carried + deferred_read_cost, definition_at(ctx, deferred.origin), log);
    if(!level.admitted())
        return name_read{ std::nullopt, eval_failure_kind::exhausted };
    const deferred_origin from = ctx.deferred[deferred.origin];
    ctx.origins.push_back(from.origin);
    std::optional<value> bound = substitute_level(ctx, from, deferred.text, log);
    ctx.origins.pop_back();
    if(bound)
        return name_read{ std::move(*bound), eval_failure_kind::none };
    const bool exhausted = ctx.session.failure == eval_failure_kind::exhausted;
    return name_read{ std::nullopt, exhausted ? eval_failure_kind::exhausted : eval_failure_kind::error };
}

// A read outside any substitution has no reader to latch the composed cause, so this level is the
// one that records it.
name_read resolve_deferred(expand_ctx &ctx, const deferred_text &deferred, std::size_t carried)
{
    const std::optional<read_anchor> reader = ctx.session.reading();
    deferred_level_sink level_log(reader ? reader->sink.get() : ctx.log);
    const name_read answer = evaluate_level(ctx, deferred, carried, level_log);
    if(!answer.failed())
        return answer;
    ctx.ok = false;
    const pugi::xml_node host = ctx.deferred[deferred.origin].host;
    const std::optional<source_location> read = reader ? std::optional<source_location>(reader->at) : std::nullopt;
    const std::optional<expansion_error> composed =
        level_log.report(host.attribute("name").value(), definition_at(ctx, deferred.origin), read);
    if(!reader && composed)
        record_terminal(ctx, *composed);
    return answer;
}

eval_failure_kind refuse_cycle(expand_ctx &ctx, const std::vector<resolve_step> &path,
                               std::string_view repeated)
{
    std::string message = "circular variable definition: ";
    for(const resolve_step &step : path)
    {
        const source_location at = definition_at(ctx, step.origin);
        message += step.name + " (" + at.file.generic_string() + ':' + std::to_string(at.line) + ") -> ";
    }
    message += repeated;
    fail(ctx, definition_at(ctx, path.back().origin), diagnostic_code::xacro_structural_error, message);
    return eval_failure_kind::error;
}

}

std::size_t record_origin(expand_ctx &ctx, pugi::xml_node host, const char *attribute,
                          const std::filesystem::path &document)
{
    const emit_origin origin = ctx.origins.empty() ? emit_origin{ document, {} } : ctx.origins.back();
    ctx.deferred.push_back(deferred_origin{ host, attr_dom_index(host, attribute), origin, document });
    return ctx.deferred.size() - 1;
}

resolver_guard::resolver_guard(expand_ctx &ctx)
    : m_ctx(ctx)
{
    chain_access::install(
        ctx.scope,
        [&ctx](const deferred_text &deferred, std::size_t carried) { return resolve_deferred(ctx, deferred, carried); },
        [&ctx](const std::vector<resolve_step> &path, std::string_view repeated) {
            return refuse_cycle(ctx, path, repeated);
        });
}

resolver_guard::~resolver_guard()
{
    chain_access::uninstall(m_ctx.scope);
}

}

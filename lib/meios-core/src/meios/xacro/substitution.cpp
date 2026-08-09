#include "eval_session.h"
#include "substitution_detail.h"

#include "meios/xacro/substitution.h"
#include "meios/xacro/evaluator_limits.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"

#include "meios/expected.h"

#include <memory>
#include <string>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace
{

// A scan that failed without recording a cause is a refusal site that never wrote
// one; the unspecified code and the empty location are what names it, rather than a
// plausible-looking record synthesized here.
unexpected<expansion_error> refuse(const detail::subst_ctx &ctx)
{
    if(ctx.terminal)
        return unexpected<expansion_error>(*ctx.terminal);
    return unexpected<expansion_error>(expansion_error{});
}

// A substitute() made outside a load has no session to charge, so it gets one that lives
// exactly as long as the call.
struct standalone_session
{
    standalone_session() : ceilings(), session(ceilings) {}

    evaluator_limits ceilings;
    detail::eval_session session;
};

}

namespace detail
{

bool scan(subst_ctx &ctx, std::string_view raw, std::string &out, std::size_t base)
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
        if(!expand_span(ctx, raw, dollar, out, cursor, base))
            return false;
    }
    out.append(raw.substr(cursor));
    return true;
}

expected<substitution, expansion_error> substitute_refined(
    std::string_view raw, const eval_scope &scope, source_stack &sources,
    const std::filesystem::path &document, eval_policy policy,
    const std::shared_ptr<evaluator_handle> &backend, eval_session &session, log_sink &log,
    const source_location &at, pugi::xml_node host, std::string_view host_text,
    std::optional<std::size_t> attr_index)
{
    subst_ctx ctx(log, sources, scope, document, policy, backend, session, at);
    ctx.host = host;
    ctx.host_text = host_text;
    ctx.attr_index = attr_index;
    std::string out;
    if(!scan(ctx, raw, out, 0))
        return refuse(ctx);
    const substitution resolved{ std::move(out) };
    return resolved;
}

expected<substitution, expansion_error> substitute_in(subst_ctx &ctx, std::string_view raw)
{
    return substitute_refined(raw, ctx.scope, ctx.sources, ctx.document, ctx.mode, ctx.backend,
                              ctx.session, ctx.log, ctx.at);
}

}

expected<substitution, expansion_error> substitute(std::string_view raw, const eval_scope &scope,
                                                   source_stack &sources,
                                                   const std::filesystem::path &document,
                                                   eval_policy policy,
                                                   const std::shared_ptr<evaluator_handle> &backend,
                                                   log_sink &log, const source_location &at)
{
    standalone_session own;
    detail::subst_ctx ctx(log, sources, scope, document, policy, backend, own.session, at);
    std::string out;
    if(!detail::scan(ctx, raw, out, 0))
        return refuse(ctx);
    const substitution resolved{ std::move(out) };
    return resolved;
}

expected<substitution, expansion_error> substitute(std::string_view raw, const eval_scope &scope,
                                                   source_stack &sources,
                                                   const std::filesystem::path &document,
                                                   eval_policy policy,
                                                   const std::shared_ptr<evaluator_handle> &backend,
                                                   log_sink &log)
{
    return substitute(raw, scope, sources, document, policy, backend, log,
                      source_location{ document, 0, 0 });
}

expected<substitution, expansion_error> substitute(std::string_view raw, const eval_scope &scope,
                                                   source_stack &sources,
                                                   const std::filesystem::path &document,
                                                   log_sink &log)
{
    return substitute(raw, scope, sources, document, eval_policy::fail, {}, log);
}

}

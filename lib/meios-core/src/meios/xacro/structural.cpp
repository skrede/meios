#include "structural_detail.h"

#include "meios/xacro/structural.h"

#include "meios/detail/text_location.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <memory>
#include <sstream>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios
{

namespace
{

void seed_document(detail::expand_ctx &ctx, std::string_view source,
                   const std::filesystem::path &document, pugi::xml_document &doc)
{
    std::error_code canon_ec;
    std::filesystem::path canonical_key = std::filesystem::weakly_canonical(document, canon_ec);
    ctx.include_stack.push_back(canon_ec ? document : canonical_key);
    ctx.scope.set_active_document(ctx.include_stack.back());
    ctx.origins.push_back(detail::emit_origin{ document, source });
    detail::seed_declared_args(ctx, doc);
}

unexpected<expansion_error> refuse(const detail::expand_ctx &ctx,
                                   const std::filesystem::path &document)
{
    return unexpected<expansion_error>(detail::terminal_of(ctx, document));
}

unexpected<expansion_error> refuse_parse(detail::expand_ctx &ctx, std::string_view source,
                                         const std::filesystem::path &document,
                                         const pugi::xml_parse_result &parsed)
{
    const source_location at = detail::offset_location(source, parsed.offset, document);
    const std::string message = std::string("xacro parse error: ") + parsed.description();
    detail::record_terminal(ctx, at, diagnostic_code::xacro_parse_error, message);
    ctx.log.log(level::error, diagnostic_code::xacro_parse_error, at, message);
    return refuse(ctx, document);
}

}

// pugixml's default parse flags never load a DTD or resolve external entities, so
// an XXE / entity-expansion payload has no effect; keep it at parse_default.
expected<expansion, expansion_error> expand(std::string_view source, eval_scope &scope,
                                            source_stack &sources,
                                            const std::filesystem::path &document,
                                            const expansion_limits &limits, eval_policy policy,
                                            const std::shared_ptr<evaluator_handle> &backend,
                                            log_sink &log)
{
    detail::expand_ctx ctx(scope, sources, limits, policy, backend, log);
    pugi::xml_document &doc = ctx.park();
    pugi::xml_parse_result parsed = doc.load_buffer(source.data(), source.size());
    if(!parsed)
        return refuse_parse(ctx, source, document, parsed);
    seed_document(ctx, source, document, doc);
    pugi::xml_document result;
    for(pugi::xml_node child : doc.children())
        if(!detail::process_node(ctx, child, result, document))
            return refuse(ctx, document);
    if(!ctx.ok)
        return refuse(ctx, document);
    std::ostringstream out;
    result.save(out, "", pugi::format_raw);
    const expansion expanded{ out.str(), ctx.session.counters.constructs };
    return expanded;
}

expected<expansion, expansion_error> expand(std::string_view source, eval_scope &scope,
                                            source_stack &sources,
                                            const std::filesystem::path &document,
                                            const expansion_limits &limits, log_sink &log)
{
    return expand(source, scope, sources, document, limits, eval_policy::fail, {}, log);
}

}

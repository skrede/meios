#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/detail/numeric.h"

#include "meios/detail/text_location.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <memory>
#include <sstream>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios
{

namespace detail
{

expand_ctx::expand_ctx(eval_scope &s, source_stack &src, const expansion_limits &lim,
                       eval_policy policy, const std::shared_ptr<evaluator_handle> &inject,
                       log_sink &lg)
    : scope(s), sources(src), limits(lim), log(lg), mode(policy), backend(inject), counters(),
      macros(), blocks(), include_stack(), owned(), owned_text(), origins(), prop_frames(),
      param_saves(), ok(true)
{
}

bool expand_ctx::charge_work(pugi::xml_node in)
{
    if(!ok)
        return false;
    if(++counters.work > limits.work)
    {
        log.log(level::error, diagnostic_code::expansion_budget_exceeded, locate(*this, in),
                "expansion budget exceeded: work limit of " + std::to_string(limits.work)
                    + " units reached");
        ok = false;
        return false;
    }
    return true;
}

bool expand_ctx::charge_output(pugi::xml_node in)
{
    if(!ok)
        return false;
    if(++counters.output_nodes > limits.output_nodes)
    {
        log.log(level::error, diagnostic_code::expansion_budget_exceeded, locate(*this, in),
                "expansion budget exceeded: output-node limit of "
                    + std::to_string(limits.output_nodes) + " nodes reached");
        ok = false;
        return false;
    }
    return true;
}

pugi::xml_document &expand_ctx::park()
{
    owned.push_back(std::make_unique<pugi::xml_document>());
    return *owned.back();
}

bool fail(expand_ctx &ctx, const source_location &loc, diagnostic_code code,
          const std::string &message)
{
    ctx.log.log(level::error, code, loc, message);
    ctx.ok = false;
    return false;
}

bool fail(expand_ctx &ctx, pugi::xml_node in, diagnostic_code code, const std::string &message)
{
    return fail(ctx, locate(ctx, in), code, message);
}

// A typed xacro emit always anchors on the hosting node's real position; with no
// origin on the stack, or a node pugixml never gave a buffer offset, fall back to the
// document path at 0:0 rather than dropping to a code-less, locationless emit.
source_location locate(const expand_ctx &ctx, pugi::xml_node in)
{
    if(ctx.origins.empty() || in.offset_debug() < 0)
        return source_location{ ctx.origins.empty() ? std::filesystem::path{} : ctx.origins.back().path,
                                0, 0 };
    return offset_location(ctx.origins.back().text, in.offset_debug(), ctx.origins.back().path);
}

binding classify(std::string_view text)
{
    bool ok = false;
    long long integer = parse_int(text, ok);
    if(ok)
        return binding{ value{ integer } };
    double real = parse_double(text, ok);
    if(ok)
        return binding{ value{ real } };
    return binding{ std::string(text) };
}

// The prior an ancestor frame must record is the value it actually holds, which the
// writing invocation's own parameter binding may be masking in the shared flat scope.
// When the written name is a parameter of the writing macro, use the outer value that
// invocation saved on entry instead of the live (masked) lookup.
std::optional<binding> ancestor_prior(const expand_ctx &ctx, std::string_view name)
{
    if(!ctx.param_saves.empty())
        for(const saved_binding &saved : *ctx.param_saves.back())
            if(saved.first == name)
                return saved.second;
    return ctx.scope.lookup(name);
}

// An empty (default) or "local" scope records into the current frame, so a property
// written inside a macro reverts when that macro exits (macro-local); at document scope
// (no frame) a default write records nowhere and persists. scope="parent" records into
// the caller's frame so an inner write reaches the immediate caller yet reverts when it
// exits; "global" and unknown scopes record nowhere and so persist.
void record_scoped(expand_ctx &ctx, std::string_view scope_attr, std::string_view name)
{
    std::size_t depth = ctx.prop_frames.size();
    std::size_t target = 0;
    if((scope_attr == "local" || scope_attr.empty()) && depth >= 1)
        target = depth - 1;
    else if(scope_attr == "parent" && depth >= 2)
        target = depth - 2;
    else
        return;
    prop_frame &frame = ctx.prop_frames[target];
    for(const std::pair<std::string, std::optional<binding>> &prior : frame)
        if(prior.first == name)
            return;
    frame.emplace_back(std::string(name), scope_attr == "parent" ? ancestor_prior(ctx, name)
                                                                  : ctx.scope.lookup(name));
}

}

namespace
{

bool has_substitution(std::string_view text)
{
    return text.find("$(") != std::string_view::npos
        || text.find("${") != std::string_view::npos;
}

// The main pass is single-forward, so a `$(arg x)` used above its own
// `<xacro:arg>` declaration would resolve before the default is seen. A pre-pass
// over the already-materialized tree seeds every declared literal default (unbound
// names only) so use-before-declaration resolves the way real xacro does. A default
// carrying a nested substitution is left for declare_arg to resolve at its
// declaration, where the sources and document needed to substitute it are in hand.
void seed_declared_args(eval_scope &scope, pugi::xml_node node)
{
    for(pugi::xml_node child : node.children())
    {
        if(child.type() != pugi::node_element)
            continue;
        if(std::string_view(child.name()) == "xacro:arg")
        {
            pugi::xml_attribute name = child.attribute("name");
            pugi::xml_attribute fallback = child.attribute("default");
            if(name && fallback && !scope.contains(name.value())
               && !has_substitution(fallback.value()))
                scope.set(name.value(), detail::classify(fallback.value()));
        }
        seed_declared_args(scope, child);
    }
}

}

// pugixml's default parse flags never load a DTD or resolve external entities, so
// an XXE / entity-expansion payload has no effect; keep it at parse_default.
expansion expand(std::string_view source, eval_scope &scope, source_stack &sources,
                 const std::filesystem::path &document, const expansion_limits &limits,
                 eval_policy policy, const std::shared_ptr<evaluator_handle> &backend,
                 log_sink &log)
{
    detail::expand_ctx ctx(scope, sources, limits, policy, backend, log);
    pugi::xml_document &doc = ctx.park();
    pugi::xml_parse_result parsed = doc.load_buffer(source.data(), source.size());
    if(!parsed)
    {
        log.log(level::error, diagnostic_code::xacro_parse_error,
                detail::offset_location(source, parsed.offset, document),
                std::string("xacro parse error: ") + parsed.description());
        return expansion{ false, {} };
    }
    std::error_code canon_ec;
    std::filesystem::path canonical_key = std::filesystem::weakly_canonical(document, canon_ec);
    ctx.include_stack.push_back(canon_ec ? document : canonical_key);
    scope.set_active_document(ctx.include_stack.back());
    ctx.origins.push_back(detail::emit_origin{ document, source });
    seed_declared_args(scope, doc);
    pugi::xml_document result;
    for(pugi::xml_node child : doc.children())
        if(!detail::process_node(ctx, child, result, document))
            return expansion{ false, {} };
    std::ostringstream out;
    result.save(out, "", pugi::format_raw);
    return expansion{ ctx.ok, out.str() };
}

expansion expand(std::string_view source, eval_scope &scope, source_stack &sources,
                 const std::filesystem::path &document, const expansion_limits &limits,
                 log_sink &log)
{
    return expand(source, scope, sources, document, limits, eval_policy::fail, {}, log);
}

}

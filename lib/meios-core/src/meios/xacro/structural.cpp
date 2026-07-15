#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/detail/numeric.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pugixml.hpp>

#include <string>
#include <memory>
#include <sstream>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace detail
{

expand_ctx::expand_ctx(eval_scope &s, source_stack &src, const expansion_limits &lim,
                       eval_policy policy, evaluator_handle *inject, log_sink &lg)
    : scope(s), sources(src), limits(lim), log(lg), mode(policy), backend(inject), counters(),
      macros(), blocks(), include_stack(), owned(), prop_frames(), ok(true)
{
}

bool expand_ctx::charge_work()
{
    if(!ok)
        return false;
    if(++counters.work > limits.work)
    {
        log.log(level::error,
                "expansion budget exceeded: work limit of " + std::to_string(limits.work)
                    + " units reached");
        ok = false;
        return false;
    }
    return true;
}

bool expand_ctx::charge_output()
{
    if(!ok)
        return false;
    if(++counters.output_nodes > limits.output_nodes)
    {
        log.log(level::error,
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

bool fail(expand_ctx &ctx, const std::string &message)
{
    ctx.log.log(level::error, message);
    ctx.ok = false;
    return false;
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

// scope="parent" records into the caller's frame so an inner write is visible to the
// immediate caller yet reverted when that caller exits; scope="local" records into the
// current frame; every other scope (default, global, unknown) records nowhere and so
// persists. A parent write with fewer than two frames targets the document scope, where
// persistence is the intended behavior.
void record_scoped(expand_ctx &ctx, std::string_view scope_attr, std::string_view name)
{
    std::size_t depth = ctx.prop_frames.size();
    std::size_t target = 0;
    if(scope_attr == "local" && depth >= 1)
        target = depth - 1;
    else if(scope_attr == "parent" && depth >= 2)
        target = depth - 2;
    else
        return;
    prop_frame &frame = ctx.prop_frames[target];
    for(const std::pair<std::string, std::optional<binding>> &prior : frame)
        if(prior.first == name)
            return;
    frame.emplace_back(std::string(name), ctx.scope.lookup(name));
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
                 eval_policy policy, evaluator_handle *backend, log_sink &log)
{
    detail::expand_ctx ctx(scope, sources, limits, policy, backend, log);
    pugi::xml_document &doc = ctx.park();
    pugi::xml_parse_result parsed = doc.load_buffer(source.data(), source.size());
    if(!parsed)
    {
        log.log(level::error, std::string("xacro parse error: ") + parsed.description());
        return expansion{ false, {} };
    }
    ctx.include_stack.push_back(std::filesystem::weakly_canonical(document));
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
    return expand(source, scope, sources, document, limits, eval_policy::fail, nullptr, log);
}

}

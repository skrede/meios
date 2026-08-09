#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/detail/numeric.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

bool has_substitution(std::string_view text)
{
    return text.find("$(") != std::string_view::npos
        || text.find("${") != std::string_view::npos;
}

std::optional<std::size_t> attr_dom_index(pugi::xml_node in, std::string_view name)
{
    std::size_t idx = 0;
    for(pugi::xml_attribute a : in.attributes())
    {
        if(std::string_view(a.name()) == name)
            return idx;
        ++idx;
    }
    return std::nullopt;
}

// The prior an ancestor frame must record is the value it actually holds, which the
// writing invocation's own parameter may be masking in the shared flat scope.
// When the written name is a parameter of the writing macro, use the outer value that
// invocation saved on entry instead of the live (masked) lookup.
std::optional<value> ancestor_prior(const expand_ctx &ctx, std::string_view name)
{
    if(!ctx.param_saves.empty())
        for(const saved_binding &saved : *ctx.param_saves.back())
            if(saved.first == name)
                return saved.second;
    return ctx.scope.lookup(name);
}

}

value classify(std::string_view text)
{
    bool ok = false;
    const std::int64_t integer = parse_int(text, ok);
    if(ok)
        return value{ integer };
    const double real = parse_double(text, ok);
    if(ok)
        if(std::optional<value> number = value::make_real(real))
            return *number;
    return value{ std::string(text) };
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
    for(const saved_binding &prior : frame)
        if(prior.first == name)
            return;
    frame.emplace_back(std::string(name), scope_attr == "parent" ? ancestor_prior(ctx, name)
                                                                  : ctx.scope.lookup(name));
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
                scope.set(name.value(), classify(strip_authored_markers(fallback.value())));
        }
        seed_declared_args(scope, child);
    }
}

bool define_property(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    bool ok = true;
    const std::string authored = strip_authored_markers(in.attribute("value").value());
    const value bound =
        substitute_attr_value(ctx, in, authored, document, ok, attr_dom_index(in, "value"));
    if(!ok)
        return false;
    std::string_view name = in.attribute("name").value();
    record_scoped(ctx, in.attribute("scope").value(), name);
    ctx.scope.set(name, bound);
    return true;
}

// A declared default seeds the scope only when the name is still unbound, so a
// caller override or an earlier binding wins; the declaration itself emits nothing.
// The default is resolved through substitution first, so a nested
// $(find)/$(arg)/${} default becomes real text rather than a raw literal.
bool declare_arg(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    std::string_view name = in.attribute("name").value();
    if(name.empty())
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "<xacro:arg> requires a name attribute");
    pugi::xml_attribute fallback = in.attribute("default");
    if(!fallback || ctx.scope.contains(name))
        return true;
    bool ok = true;
    const std::string authored = strip_authored_markers(fallback.value());
    const value resolved =
        substitute_attr_value(ctx, in, authored, document, ok, attr_dom_index(in, "default"));
    if(!ok)
        return false;
    ctx.scope.set(name, resolved);
    return true;
}

}

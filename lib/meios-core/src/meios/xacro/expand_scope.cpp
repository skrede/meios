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

bool needs_lexing(std::string_view text)
{
    return text.find("$$") != std::string_view::npos
        || text.find("$(") != std::string_view::npos
        || text.find("${") != std::string_view::npos;
}

bool take_seeded(expand_ctx &ctx, std::string_view name)
{
    const auto lent = ctx.seeded_args.find(name);
    if(lent == ctx.seeded_args.end())
        return false;
    ctx.seeded_args.erase(lent);
    return true;
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

// The number kinds are tried before the boolean spellings, which is what leaves "1" an
// integer rather than a true; the reference orders its own literal conversion the same way,
// and only these four spellings ever reach the boolean arm. A description passing
// no_prefix="false" into a macro and testing it for truth means the opposite of what a
// non-empty string would mean, so the conversion is what keeps the two readings the same.
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
    if(text == "true" || text == "True")
        return value{ true };
    if(text == "false" || text == "False")
        return value{ false };
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

// The main pass is single-forward, so a `$(arg x)` used above its own `<xacro:arg>` declaration
// would resolve before the default is seen. Only a default that lexes to itself is lent ahead;
// every other default is left for its declaration, where it is lexed.
void seed_declared_args(expand_ctx &ctx, pugi::xml_node node)
{
    for(pugi::xml_node child : node.children())
    {
        if(child.type() != pugi::node_element)
            continue;
        if(std::string_view(child.name()) == "xacro:arg")
        {
            pugi::xml_attribute name = child.attribute("name");
            pugi::xml_attribute fallback = child.attribute("default");
            if(name && fallback && !ctx.scope.has_argument(name.value()) && !needs_lexing(fallback.value()))
            {
                ctx.scope.bind_argument(name.value(), value{strip_authored_markers(fallback.value())});
                ctx.seeded_args.emplace(name.value());
            }
        }
        seed_declared_args(ctx, child);
    }
}

// The reference's grab_property treats the written value and the fallback as mutually exclusive
// and binds a fallback only where no property of the name exists; an argument of the same name
// does not suppress it. A property carrying neither attribute binds the empty string.
bool define_property(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    pugi::xml_attribute written = in.attribute("value");
    pugi::xml_attribute fallback = in.attribute("default");
    if(written && fallback)
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "<xacro:property> writes a value attribute and a default attribute together, "
                    "which are mutually exclusive");
    std::string_view name = in.attribute("name").value();
    if(fallback && ctx.scope.contains(name))
        return true;
    const char *chosen = fallback ? "default" : "value";
    bool ok = true;
    const std::string authored = strip_authored_markers(in.attribute(chosen).value());
    const value bound =
        substitute_attr_value(ctx, in, authored, document, ok, attr_dom_index(in, chosen));
    if(!ok)
        return false;
    record_scoped(ctx, in.attribute("scope").value(), name);
    ctx.scope.set(name, bound);
    return true;
}

// The first declaration the walk reaches binds, as the reference's does, so a caller override or
// an earlier reached declaration wins; a value the pre-pass lent does not count as a binding.
bool declare_arg(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    std::string_view name = in.attribute("name").value();
    if(name.empty())
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "<xacro:arg> requires a name attribute");
    pugi::xml_attribute fallback = in.attribute("default");
    if(!fallback || (!take_seeded(ctx, name) && ctx.scope.has_argument(name)))
        return true;
    bool ok = true;
    const std::string authored = strip_authored_markers(fallback.value());
    const value resolved =
        substitute_arg_value(ctx, in, authored, document, ok, attr_dom_index(in, "default"));
    if(!ok)
        return false;
    ctx.scope.bind_argument(name, resolved);
    return true;
}

}

#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
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

// The first declaration the walk reaches binds, as the reference's does, so a caller override or
// an earlier reached declaration wins; a value the pre-pass lent does not count as a binding, and
// is unbound first so the default reads the table the reference holds, which lacks the name.
bool declare_arg(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    std::string_view name = in.attribute("name").value();
    if(name.empty())
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "<xacro:arg> requires a name attribute");
    pugi::xml_attribute fallback = in.attribute("default");
    if(!fallback)
        return true;
    const bool lent = take_seeded(ctx, name);
    if(lent)
        ctx.scope.unbind_argument(name);
    else if(ctx.scope.has_argument(name))
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

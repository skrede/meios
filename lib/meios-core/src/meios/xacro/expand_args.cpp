#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"

#include <pugixml.hpp>

#include <string>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

// An argument binds where the walk reaches its first declaration, unless a caller supplied it or an
// earlier reached declaration bound it.
bool declare_arg(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    std::string_view name = in.attribute("name").value();
    pugi::xml_attribute fallback = in.attribute("default");
    if(!fallback)
        return true;
    if(ctx.scope.arguments().contains(name))
        return true;
    bool ok = true;
    const std::string authored = strip_authored_markers(fallback.value());
    const value resolved =
        substitute_arg_value(ctx, in, authored, document, ok, attr_dom_index(in, "default"));
    if(!ok)
        return false;
    ctx.scope.arguments().set(name, resolved);
    return true;
}

}

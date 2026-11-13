#include "xml_name.h"
#include "macro_tables.h"
#include "dispatch_view.h"
#include "xacro_attributes.h"
#include "structural_detail.h"

#include "meios/xacro/structural.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

std::string authored_tag(const dispatch_view &view)
{
    return '<' + std::string(view.node.name()) + '>';
}

// The name selects a macro and nothing else: the call never becomes another construct, and a call
// naming call is the call again, its macro attribute consumed.
bool call_by_name(expand_ctx &ctx, const dispatch_view &view, pugi::xml_node out, const std::filesystem::path &document)
{
    if(*view_attribute(view, "macro").value() == '\0')
        return fail(ctx, view.node, diagnostic_code::xacro_structural_error, authored_tag(view) + " names no macro in its macro attribute");
    bool ok = true;
    const std::string name = evaluated_text(ctx, view, "macro", document, ok);
    return ok && call_macro(ctx, renamed(view, "xacro:" + name, "macro"), out, document);
}

}

std::string evaluated_text(expand_ctx &ctx, const dispatch_view &view, std::string_view attr, const std::filesystem::path &document, bool &ok)
{
    const std::string raw = strip_authored_markers(view_attribute(view, attr).value());
    return strip_container_marker(substitute_attr(ctx, view.node, raw, document, ok, attr_dom_index(view.node, attr)));
}

// Only a name reaches the output, so evaluated text can never open, close or attribute an element.
std::string evaluated_name(expand_ctx &ctx, const dispatch_view &view, std::string_view attr, const std::filesystem::path &document, bool &ok)
{
    const std::string text = evaluated_text(ctx, view, attr, document, ok);
    if(!ok)
        return {};
    const std::string name(trim_xml_space(text));
    const std::string head = '<' + view.name + "> evaluates its " + std::string(attr) + " to ";
    if(name.empty())
        ok = fail(ctx, view.node, diagnostic_code::xacro_structural_error, head + "an empty name");
    else if(!is_xml_name(name))
        ok = fail(ctx, view.node, diagnostic_code::xacro_structural_error, head + '\'' + name + "', which is not an XML name");
    else if(name.size() != text.size())
        ctx.log.log(level::warn, locate(ctx, view.node), head + '\'' + text + "' and uses '" + name + "', without the white space around it");
    return name;
}

bool call_macro(expand_ctx &ctx, const dispatch_view &view, pugi::xml_node out, const std::filesystem::path &document)
{
    if(view.name == "xacro:call")
        return check_attributes(ctx, view) && call_by_name(ctx, view, out, document);
    const std::string_view macro = std::string_view(view.name).substr(6);
    const std::optional<resolved_macro> found = resolve_macro(ctx.macros, ctx.scope, macro);
    if(found)
        return instantiate_macro(ctx, *found, view, out, document);
    if(view.dropped.empty())
        return fail(ctx, view.node, diagnostic_code::xacro_structural_error, "unknown xacro element <" + view.name + '>');
    return fail(ctx, view.node, diagnostic_code::xacro_structural_error,
                authored_tag(view) + " names '" + std::string(macro) + "', which is not a macro in scope");
}

bool rename_element(expand_ctx &ctx, const dispatch_view &view, pugi::xml_node out, const std::filesystem::path &document)
{
    bool ok = true;
    const std::string name = evaluated_name(ctx, view, "xacro:name", document, ok);
    return ok && dispatch_element(ctx, renamed(view, name, "xacro:name"), out, document);
}

}

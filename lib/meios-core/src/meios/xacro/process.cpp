#include "macro_tables.h"
#include "boolean_value.h"
#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/container_marker.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

bool conditional(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                 const std::filesystem::path &document)
{
    const std::string_view tag = in.name();
    const std::string authored = strip_authored_markers(in.attribute("value").value());
    bool ok                    = true;
    const value read           = substitute_condition_value(ctx, in, authored, document, ok, attr_dom_index(in, "value"));
    if(!ok)
        return false;
    const std::optional<bool> truth = get_boolean_value(read);
    if(!truth)
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    std::string(tag) + " value=\"" + authored + "\" evaluates to \"" + read.text().value_or("") + "\", which is not a boolean");
    if(*truth == (tag == "xacro:if"))
        return process_children(ctx, in, out, document);
    return true;
}

bool dispatch_element(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                      const std::filesystem::path &document)
{
    std::string_view name = in.name();
    if(name == "xacro:property")
        return define_property(ctx, in, document);
    if(name == "xacro:macro")
        return define_macro(ctx, in);
    if(name == "xacro:arg")
        return declare_arg(ctx, in, document);
    if(name == "xacro:include")
        return expand_include(ctx, in, out, document);
    if(name == "xacro:if" || name == "xacro:unless")
        return conditional(ctx, in, out, document);
    if(name == "xacro:insert_block")
        return insert_block(ctx, in, out, document);
    if(name.rfind("xacro:", 0) != 0)
        return emit_element(ctx, in, out, document);
    const std::optional<resolved_macro> found = resolve_macro(ctx.macros, ctx.scope, name.substr(6));
    if(found)
        return instantiate_macro(ctx, *found, in, out, document);
    return fail(ctx, in, diagnostic_code::xacro_structural_error,
                "unknown xacro element <" + std::string(name) + '>');
}

}

std::string strip_container_marker(std::string text)
{
    for(char marker : container_markers)
        text.erase(std::remove(text.begin(), text.end(), marker), text.end());
    return text;
}

std::string strip_authored_markers(std::string text)
{
    for(char marker : container_markers)
        text.erase(std::remove(text.begin(), text.end(), marker), text.end());
    return text;
}

bool process_node(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document)
{
    depth_guard level(ctx, in);
    if(!level.admitted() || !ctx.charge_work(in))
        return false;
    pugi::xml_node_type kind = in.type();
    if(kind == pugi::node_pcdata || kind == pugi::node_cdata)
    {
        bool ok = true;
        std::string text = strip_container_marker(substitute_attr(ctx, in, in.value(), document, ok));
        if(!ok)
            return false;
        out.append_child(kind).set_value(text.c_str());
        return true;
    }
    if(kind != pugi::node_element)
        return true;
    return dispatch_element(ctx, in, out, document);
}

bool process_children(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                      const std::filesystem::path &document)
{
    for(pugi::xml_node child : in.children())
        if(!process_node(ctx, child, out, document))
            return false;
    return true;
}

}

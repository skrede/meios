#include "boolean_value.h"
#include "dispatch_view.h"
#include "namespace_lift.h"
#include "xacro_attributes.h"
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

bool conditional(expand_ctx &ctx, const dispatch_view &view, pugi::xml_node out,
                 const std::filesystem::path &document)
{
    const pugi::xml_node in    = view.node;
    const std::string_view tag = view.name;
    const std::string authored = strip_authored_markers(in.attribute("value").value());
    bool ok                    = true;
    const value read           = substitute_condition_value(ctx, in, authored, document, ok, attr_dom_index(in, "value"));
    if(!ok)
        return false;
    const std::optional<bool> truth = get_boolean_value(read);
    if(!truth)
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    std::string(tag) + " value=\"" + authored + "\" evaluates to \"" + read.text().value_or("") + "\", which is not a boolean");
    if(*truth != (tag == "xacro:if"))
        return true;
    const lift_target dropped(ctx, {});
    return process_children(ctx, in, out, document);
}

bool copy_text(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out, const std::filesystem::path &document)
{
    std::string text;
    if(in.type() == pugi::node_cdata)
        text = strip_authored_markers(in.value());
    else
    {
        bool ok = true;
        text = strip_container_marker(substitute_attr(ctx, in, in.value(), document, ok));
        if(!ok)
            return false;
    }
    out.append_child(in.type()).set_value(text.c_str());
    return true;
}

}

bool dispatch_element(expand_ctx &ctx, const dispatch_view &view, pugi::xml_node out,
                      const std::filesystem::path &document)
{
    const std::string_view name = view.name;
    if(name == "xacro:property")
        return check_attributes(ctx, view) && define_property(ctx, view.node, document);
    if(name == "xacro:macro")
        return check_attributes(ctx, view) && define_macro(ctx, view.node);
    if(name == "xacro:arg")
        return check_attributes(ctx, view) && declare_arg(ctx, view.node, document);
    if(name == "xacro:include")
        return check_attributes(ctx, view) && expand_include(ctx, view.node, out, document);
    if(name == "xacro:if" || name == "xacro:unless")
        return check_attributes(ctx, view) && conditional(ctx, view, out, document);
    if(name == "xacro:insert_block")
        return check_attributes(ctx, view) && insert_block(ctx, view.node, out, document);
    if(name == "xacro:element")
        return check_attributes(ctx, view) && rename_element(ctx, view, out, document);
    if(name == "xacro:attribute")
        return check_attributes(ctx, view) && set_attribute(ctx, view, out, document);
    if(!name.starts_with("xacro:"))
        return emit_element(ctx, view.node, view.name, out, document);
    return call_macro(ctx, view, out, document);
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
        return copy_text(ctx, in, out, document);
    if(kind != pugi::node_element)
        return true;
    return dispatch_element(ctx, authored_view(in), out, document);
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

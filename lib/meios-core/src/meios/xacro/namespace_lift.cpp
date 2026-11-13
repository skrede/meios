#include "namespace_lift.h"
#include "structural_detail.h"

#include "meios/diagnostic/level.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

void lift_one(expand_ctx &ctx, pugi::xml_node from, const char *name, const std::string &value)
{
    pugi::xml_node onto = ctx.lift_onto;
    if(!onto)
        return;
    pugi::xml_attribute held = onto.attribute(name);
    if(!held)
        onto.append_attribute(name).set_value(value.c_str());
    else if(held.value() != value)
        ctx.log.log(level::warn, locate(ctx, from),
                    "namespace prefix '" + std::string(std::string_view(name).substr(6)) + "' is already bound to \"" + held.value() + "\" on <" + onto.name() +
                            ">, so its declaration as \"" + value + "\" here is not lifted");
}

}

bool xacro_namespace_attribute(std::string_view name)
{
    return name.starts_with("xacro:");
}

bool xacro_namespace_declaration(std::string_view name)
{
    return name == "xmlns:xacro";
}

lift_target::lift_target(expand_ctx &ctx, pugi::xml_node onto)
        : m_ctx(ctx)
        , m_enclosing(ctx.lift_onto)
{
    m_ctx.lift_onto = onto;
}

lift_target::~lift_target()
{
    m_ctx.lift_onto = m_enclosing;
}

bool read_wrapper_attributes(expand_ctx &ctx, pugi::xml_node from, const std::filesystem::path &document, std::string_view evaluated_elsewhere)
{
    std::size_t attr_index = 0;
    for(pugi::xml_attribute attr : from.attributes())
    {
        const std::string_view name = attr.name();
        if(!xacro_namespace_attribute(name) && name != evaluated_elsewhere)
        {
            bool ok                 = true;
            const std::string value = strip_container_marker(substitute_attr(ctx, from, attr.value(), document, ok, attr_index));
            if(!ok)
                return false;
            if(name.starts_with("xmlns:") && !xacro_namespace_declaration(name))
                lift_one(ctx, from, attr.name(), value);
        }
        ++attr_index;
    }
    return true;
}

std::optional<std::string> target_namespace(expand_ctx &ctx, pugi::xml_node root, pugi::xml_attribute written, const std::filesystem::path &document, bool &ok)
{
    if(std::string_view(written.value()).empty())
        return std::nullopt;
    std::string uri = strip_container_marker(substitute_attr(ctx, root, written.value(), document, ok, attr_dom_index(root, written.name())));
    if(!ok)
        return std::nullopt;
    return uri;
}

void declare_default_namespace(pugi::xml_node root, const std::string &uri)
{
    pugi::xml_attribute declared = root.attribute("xmlns");
    if(!declared)
        declared = root.append_attribute("xmlns");
    declared.set_value(uri.c_str());
}

}

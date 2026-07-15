#include "structural_detail.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pugixml.hpp>

#include <map>
#include <string>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

bool emit_element(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document)
{
    if(!ctx.charge_output())
        return false;
    pugi::xml_node element = out.append_child(in.name());
    for(pugi::xml_attribute attr : in.attributes())
    {
        if(std::string_view(attr.name()) == "xmlns:xacro")
            continue;
        bool ok = true;
        std::string value = strip_container_marker(substitute_attr(ctx, attr.value(), document, ok));
        if(!ok)
            return false;
        element.append_attribute(attr.name()).set_value(value.c_str());
    }
    return process_children(ctx, in, element, document);
}

bool insert_block(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document)
{
    std::string name = in.attribute("name").value();
    auto found = ctx.blocks.find(name);
    if(found == ctx.blocks.end())
        return fail(ctx, "xacro:insert_block references unknown block '" + name + '\'');
    if(found->second.children)
        return process_children(ctx, found->second.source, out, document);
    return process_node(ctx, found->second.source, out, document);
}

}

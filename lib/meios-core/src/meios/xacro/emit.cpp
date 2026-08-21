#include "structural_detail.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <map>
#include <string>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

// Every element the copy will land in output is charged, so a block expanded once and inserted
// many times cannot outgrow the emitted-node ceiling the way a shallow-but-wide fan-out would.
bool charge_copy(expand_ctx &ctx, pugi::xml_node in)
{
    if(in.type() == pugi::node_element && !ctx.charge_output(in))
        return false;
    for(pugi::xml_node child : in.children())
        if(!charge_copy(ctx, child))
            return false;
    return true;
}

bool copy_charged(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out)
{
    if(!charge_copy(ctx, in))
        return false;
    out.append_copy(in);
    return true;
}

}

bool emit_element(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document)
{
    if(!ctx.charge_output(in))
        return false;
    pugi::xml_node element = out.append_child(in.name());
    std::size_t attr_index = 0;
    for(pugi::xml_attribute attr : in.attributes())
    {
        if(std::string_view(attr.name()) == "xmlns:xacro")
        {
            ++attr_index;
            continue;
        }
        bool ok = true;
        std::string value =
            strip_container_marker(substitute_attr(ctx, in, attr.value(), document, ok, attr_index));
        if(!ok)
            return false;
        element.append_attribute(attr.name()).set_value(value.c_str());
        ++attr_index;
    }
    return process_children(ctx, in, element, document);
}

// The bound subtree was expanded at the call site, so insertion copies it rather than walking it
// again: a second walk would evaluate a substitution the first pass legitimately produced as text,
// and would resolve a forwarded insert against the binding that produced it.
bool insert_block(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out)
{
    std::string name = in.attribute("name").value();
    auto found = ctx.blocks.find(name);
    if(found == ctx.blocks.end())
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "xacro:insert_block references unknown block '" + name + '\'');
    if(!found->second.children)
        return copy_charged(ctx, found->second.source, out);
    for(pugi::xml_node child : found->second.source.children())
        if(!copy_charged(ctx, child, out))
            return false;
    return true;
}

}

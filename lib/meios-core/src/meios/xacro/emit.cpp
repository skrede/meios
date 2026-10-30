#include "structural_detail.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

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

}

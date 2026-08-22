#include "writer_detail.h"

#include "meios/records/material.h"

#include "meios/xacro/detail/numeric.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <optional>

namespace meios::detail
{

namespace
{

void append_color(pugi::xml_node node, const rgba<double> &color)
{
    const std::string value = print_double(color.r) + ' ' + print_double(color.g) + ' '
                            + print_double(color.b) + ' ' + print_double(color.a);
    node.append_child("color").append_attribute("rgba").set_value(value.c_str());
}

}

void append_material(pugi::xml_node parent, const material<double> &mat,
                     std::vector<reference_record> &refs)
{
    pugi::xml_node node = parent.append_child("material");
    node.append_attribute("name").set_value(mat.name.c_str());
    if(mat.color)
        append_color(node, *mat.color);
    if(mat.texture)
    {
        node.append_child("texture").append_attribute("filename").set_value(mat.texture->c_str());
        refs.push_back(reference_record{ *mat.texture, mat.resolved_texture, true });
    }
}

void append_material_ref(pugi::xml_node parent, const std::string &name)
{
    parent.append_child("material").append_attribute("name").set_value(name.c_str());
}

}

#include "writer_detail.h"

#include "meios/records/geometry.h"

#include "meios/xacro/detail/numeric.h"

#include <pugixml.hpp>

#include <vector>
#include <variant>

namespace meios::detail
{

namespace
{

void append_mesh(pugi::xml_node parent, const mesh<double> &shape, std::vector<reference_record> &refs)
{
    pugi::xml_node node = parent.append_child("mesh");
    node.append_attribute("filename").set_value(shape.filename.c_str());
    node.append_attribute("scale").set_value(xyz_string(shape.scale).c_str());
    refs.push_back(reference_record{ shape.filename, shape.resolved_path, false });
}

void append_box(pugi::xml_node parent, const box<double> &shape)
{
    parent.append_child("box").append_attribute("size").set_value(xyz_string(shape.size).c_str());
}

void append_cylinder(pugi::xml_node parent, const cylinder<double> &shape)
{
    pugi::xml_node node = parent.append_child("cylinder");
    node.append_attribute("radius").set_value(print_double(shape.radius).c_str());
    node.append_attribute("length").set_value(print_double(shape.length).c_str());
}

void append_sphere(pugi::xml_node parent, const sphere<double> &shape)
{
    parent.append_child("sphere").append_attribute("radius").set_value(print_double(shape.radius).c_str());
}

}

void append_geometry(pugi::xml_node parent, const geometry<double> &geom,
                     std::vector<reference_record> &refs)
{
    pugi::xml_node node = parent.append_child("geometry");
    if(const mesh<double> *mesh_shape = std::get_if<mesh<double>>(&geom.shape))
        append_mesh(node, *mesh_shape, refs);
    else if(const box<double> *box_shape = std::get_if<box<double>>(&geom.shape))
        append_box(node, *box_shape);
    else if(const cylinder<double> *cylinder_shape = std::get_if<cylinder<double>>(&geom.shape))
        append_cylinder(node, *cylinder_shape);
    else if(const sphere<double> *sphere_shape = std::get_if<sphere<double>>(&geom.shape))
        append_sphere(node, *sphere_shape);
}

}

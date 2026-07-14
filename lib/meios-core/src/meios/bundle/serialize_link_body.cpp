#include "writer_detail.h"

#include "meios/records/link.h"
#include "meios/records/visual.h"
#include "meios/records/collision.h"
#include "meios/records/inertial.h"

#include "meios/math/inertia.h"
#include "meios/math/vector3.h"
#include "meios/math/transform.h"
#include "meios/math/rotations.h"

#include "meios/xacro/detail/numeric.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <optional>

namespace meios::detail
{

std::string xyz_string(const vector3<double> &v)
{
    return print_double(v.x) + ' ' + print_double(v.y) + ' ' + print_double(v.z);
}

std::string rpy_string(const rpy<double> &r)
{
    return print_double(r.roll) + ' ' + print_double(r.pitch) + ' ' + print_double(r.yaw);
}

void append_origin(pugi::xml_node parent, const transform<double> &tf)
{
    pugi::xml_node node = parent.append_child("origin");
    node.append_attribute("xyz").set_value(xyz_string(tf.translation).c_str());
    node.append_attribute("rpy").set_value(rpy_string(tf.rotation).c_str());
}

namespace
{

void append_inertia(pugi::xml_node parent, const inertia<double> &tensor)
{
    pugi::xml_node node = parent.append_child("inertia");
    node.append_attribute("ixx").set_value(print_double(tensor.ixx).c_str());
    node.append_attribute("ixy").set_value(print_double(tensor.ixy).c_str());
    node.append_attribute("ixz").set_value(print_double(tensor.ixz).c_str());
    node.append_attribute("iyy").set_value(print_double(tensor.iyy).c_str());
    node.append_attribute("iyz").set_value(print_double(tensor.iyz).c_str());
    node.append_attribute("izz").set_value(print_double(tensor.izz).c_str());
}

void append_inertial(pugi::xml_node parent, const inertial<double> &body)
{
    pugi::xml_node node = parent.append_child("inertial");
    append_origin(node, body.origin);
    node.append_child("mass").append_attribute("value").set_value(print_double(body.mass).c_str());
    append_inertia(node, body.tensor);
}

void append_visual(pugi::xml_node parent, const visual<double> &vis, std::vector<reference_record> &refs)
{
    pugi::xml_node node = parent.append_child("visual");
    append_origin(node, vis.origin);
    append_geometry(node, vis.geom, refs);
    if(vis.material_ref)
        append_material_ref(node, *vis.material_ref);
}

void append_collision(pugi::xml_node parent, const collision<double> &col,
                      std::vector<reference_record> &refs)
{
    pugi::xml_node node = parent.append_child("collision");
    append_origin(node, col.origin);
    append_geometry(node, col.geom, refs);
}

}

void append_link(pugi::xml_node parent, const link<double> &node,
                 std::vector<reference_record> &refs)
{
    pugi::xml_node element = parent.append_child("link");
    element.append_attribute("name").set_value(node.name.c_str());
    if(node.body)
        append_inertial(element, *node.body);
    for(const visual<double> &vis : node.visuals)
        append_visual(element, vis, refs);
    for(const collision<double> &col : node.collisions)
        append_collision(element, col, refs);
}

}

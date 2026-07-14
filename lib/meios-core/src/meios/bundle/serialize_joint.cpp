#include "writer_detail.h"

#include "meios/records/joint.h"
#include "meios/records/joint_extras.h"

#include "meios/xacro/detail/numeric.h"

#include <pugixml.hpp>

#include <string>
#include <optional>

namespace meios::detail
{

namespace
{

std::string kind_to_string(joint_kind kind)
{
    switch(kind)
    {
        case joint_kind::revolute:   return "revolute";
        case joint_kind::continuous: return "continuous";
        case joint_kind::prismatic:  return "prismatic";
        case joint_kind::floating:   return "floating";
        case joint_kind::planar:     return "planar";
        case joint_kind::fixed:      return "fixed";
    }
    return "fixed";
}

bool needs_axis(joint_kind kind)
{
    return kind == joint_kind::revolute || kind == joint_kind::continuous
        || kind == joint_kind::prismatic || kind == joint_kind::planar;
}

void append_limits(pugi::xml_node parent, const joint_limits<double> &lim)
{
    pugi::xml_node node = parent.append_child("limit");
    node.append_attribute("lower").set_value(print_double(lim.lower).c_str());
    node.append_attribute("upper").set_value(print_double(lim.upper).c_str());
    node.append_attribute("effort").set_value(print_double(lim.effort).c_str());
    node.append_attribute("velocity").set_value(print_double(lim.velocity).c_str());
}

void append_mimic(pugi::xml_node parent, const mimic &couple)
{
    pugi::xml_node node = parent.append_child("mimic");
    node.append_attribute("joint").set_value(couple.joint.c_str());
    node.append_attribute("multiplier").set_value(print_double(couple.multiplier).c_str());
    node.append_attribute("offset").set_value(print_double(couple.offset).c_str());
}

void append_dynamics(pugi::xml_node parent, const dynamics<double> &dyn)
{
    pugi::xml_node node = parent.append_child("dynamics");
    node.append_attribute("damping").set_value(print_double(dyn.damping).c_str());
    node.append_attribute("friction").set_value(print_double(dyn.friction).c_str());
}

void append_safety(pugi::xml_node parent, const safety_controller<double> &safety)
{
    pugi::xml_node node = parent.append_child("safety_controller");
    node.append_attribute("soft_lower_limit").set_value(print_double(safety.soft_lower_limit).c_str());
    node.append_attribute("soft_upper_limit").set_value(print_double(safety.soft_upper_limit).c_str());
    node.append_attribute("k_position").set_value(print_double(safety.k_position).c_str());
    node.append_attribute("k_velocity").set_value(print_double(safety.k_velocity).c_str());
}

void append_calibration(pugi::xml_node parent, const calibration<double> &calib)
{
    pugi::xml_node node = parent.append_child("calibration");
    if(calib.rising)
        node.append_attribute("rising").set_value(print_double(*calib.rising).c_str());
    if(calib.falling)
        node.append_attribute("falling").set_value(print_double(*calib.falling).c_str());
}

void append_extras(pugi::xml_node node, const joint<double> &edge)
{
    if(edge.limits)
        append_limits(node, *edge.limits);
    if(edge.couple)
        append_mimic(node, *edge.couple);
    if(edge.dyn)
        append_dynamics(node, *edge.dyn);
    if(edge.safety)
        append_safety(node, *edge.safety);
    if(edge.calib)
        append_calibration(node, *edge.calib);
}

}

void append_joint(pugi::xml_node parent, const joint<double> &edge)
{
    pugi::xml_node node = parent.append_child("joint");
    node.append_attribute("name").set_value(edge.name.c_str());
    node.append_attribute("type").set_value(kind_to_string(edge.kind).c_str());
    node.append_child("parent").append_attribute("link").set_value(edge.parent.c_str());
    node.append_child("child").append_attribute("link").set_value(edge.child.c_str());
    append_origin(node, edge.origin);
    if(needs_axis(edge.kind))
        node.append_child("axis").append_attribute("xyz").set_value(xyz_string(edge.axis).c_str());
    append_extras(node, edge);
}

}

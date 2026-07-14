#include "urdf_detail.h"

#include "meios/records/joint.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

joint_kind kind_from_string(std::string_view text)
{
    if(text == "revolute")
        return joint_kind::revolute;
    if(text == "continuous")
        return joint_kind::continuous;
    if(text == "prismatic")
        return joint_kind::prismatic;
    if(text == "floating")
        return joint_kind::floating;
    if(text == "planar")
        return joint_kind::planar;
    return joint_kind::fixed;
}

vector3<double> read_axis(pugi::xml_node node, parse_context &ctx, const source_location &loc)
{
    const pugi::xml_node axis = node.child("axis");
    if(!axis)
        return vector3<double>{ 1.0, 0.0, 0.0 };
    return read_vec3(axis.attribute("xyz").value(), ctx, loc, "axis");
}

std::optional<joint_limits<double>> read_limits(pugi::xml_node node, parse_context &ctx,
                                                const source_location &loc)
{
    const pugi::xml_node limit = node.child("limit");
    if(!limit)
        return std::nullopt;
    joint_limits<double> out{};
    read_finite(limit.attribute("lower").value(), out.lower, ctx, loc, "lower");
    read_finite(limit.attribute("upper").value(), out.upper, ctx, loc, "upper");
    read_finite(limit.attribute("effort").value(), out.effort, ctx, loc, "effort");
    read_finite(limit.attribute("velocity").value(), out.velocity, ctx, loc, "velocity");
    return out;
}

std::optional<mimic> read_mimic(pugi::xml_node node, parse_context &ctx, const source_location &loc)
{
    const pugi::xml_node source = node.child("mimic");
    if(!source)
        return std::nullopt;
    mimic out{};
    out.joint = source.attribute("joint").value();
    out.multiplier = 1.0;
    out.offset = 0.0;
    read_finite(source.attribute("multiplier").value(), out.multiplier, ctx, loc, "multiplier");
    read_finite(source.attribute("offset").value(), out.offset, ctx, loc, "offset");
    return out;
}

std::optional<dynamics<double>> read_dynamics(pugi::xml_node node, parse_context &ctx,
                                              const source_location &loc)
{
    const pugi::xml_node source = node.child("dynamics");
    if(!source)
        return std::nullopt;
    dynamics<double> out{};
    read_finite(source.attribute("damping").value(), out.damping, ctx, loc, "damping");
    read_finite(source.attribute("friction").value(), out.friction, ctx, loc, "friction");
    return out;
}

std::optional<safety_controller<double>> read_safety(pugi::xml_node node, parse_context &ctx,
                                                     const source_location &loc)
{
    const pugi::xml_node source = node.child("safety_controller");
    if(!source)
        return std::nullopt;
    safety_controller<double> out{};
    read_finite(source.attribute("soft_lower_limit").value(), out.soft_lower_limit, ctx, loc, "soft_lower_limit");
    read_finite(source.attribute("soft_upper_limit").value(), out.soft_upper_limit, ctx, loc, "soft_upper_limit");
    read_finite(source.attribute("k_position").value(), out.k_position, ctx, loc, "k_position");
    read_finite(source.attribute("k_velocity").value(), out.k_velocity, ctx, loc, "k_velocity");
    return out;
}

std::optional<calibration<double>> read_calibration(pugi::xml_node node, parse_context &ctx,
                                                    const source_location &loc)
{
    const pugi::xml_node source = node.child("calibration");
    if(!source)
        return std::nullopt;
    calibration<double> out{};
    double value = 0.0;
    if(read_finite(source.attribute("rising").value(), value, ctx, loc, "rising"))
        out.rising = value;
    if(read_finite(source.attribute("falling").value(), value, ctx, loc, "falling"))
        out.falling = value;
    return out;
}

}

joint<double> extract_joint(pugi::xml_node node, std::string_view text,
                            const std::filesystem::path &file, parse_context &ctx)
{
    const source_location loc = node_location(node, text, file);
    joint<double> out{};
    out.name = node.attribute("name").value();
    out.kind = kind_from_string(node.attribute("type").value());
    out.parent = node.child("parent").attribute("link").value();
    out.child = node.child("child").attribute("link").value();
    out.origin = read_transform(node.child("origin"), ctx, loc);
    out.axis = read_axis(node, ctx, loc);
    out.limits = read_limits(node, ctx, loc);
    out.couple = read_mimic(node, ctx, loc);
    out.dyn = read_dynamics(node, ctx, loc);
    out.safety = read_safety(node, ctx, loc);
    out.calib = read_calibration(node, ctx, loc);
    out.closes_loop = false;
    out.origin_loc = loc;
    return out;
}

}

#include "urdf_detail.h"

#include "meios/records/joint.h"

#include <optional>

namespace meios::detail
{

// The wiki states a default for the mimic offset and none for the multiplier; the profile
// keeps one on its own authority and records the reasoning where a reader can find it.
std::optional<mimic> read_mimic(pugi::xml_node node, parse_context &ctx,
                                const source_location &loc)
{
    const pugi::xml_node source = node.child("mimic");
    if(!source)
        return std::nullopt;
    mimic out{};
    out.joint = source.attribute("joint").value();
    out.multiplier = 1.0;
    out.offset = 0.0;
    bool ok = read_optional(source, "multiplier", out.multiplier, ctx, loc);
    ok = read_optional(source, "offset", out.offset, ctx, loc) && ok;
    if(!ok)
        return std::nullopt;
    return out;
}

std::optional<dynamics<double>> read_dynamics(pugi::xml_node node, parse_context &ctx,
                                              const source_location &loc)
{
    const pugi::xml_node source = node.child("dynamics");
    if(!source)
        return std::nullopt;
    dynamics<double> out{};
    bool ok = read_optional(source, "damping", out.damping, ctx, loc);
    ok = read_optional(source, "friction", out.friction, ctx, loc) && ok;
    if(!ok)
        return std::nullopt;
    return out;
}

std::optional<safety_controller<double>> read_safety(pugi::xml_node node, parse_context &ctx,
                                                     const source_location &loc)
{
    const pugi::xml_node source = node.child("safety_controller");
    if(!source)
        return std::nullopt;
    safety_controller<double> out{};
    bool ok = read_optional(source, "soft_lower_limit", out.soft_lower_limit, ctx, loc);
    ok = read_optional(source, "soft_upper_limit", out.soft_upper_limit, ctx, loc) && ok;
    ok = read_optional(source, "k_position", out.k_position, ctx, loc) && ok;
    ok = read_optional(source, "k_velocity", out.k_velocity, ctx, loc) && ok;
    if(!ok)
        return std::nullopt;
    return out;
}

// rising and falling are optional with no stated default, so each answers for itself: an
// absent one leaves no value and is not a violation, an unreadable one is said out loud.
std::optional<calibration<double>> read_calibration(pugi::xml_node node, parse_context &ctx,
                                                    const source_location &loc)
{
    const pugi::xml_node source = node.child("calibration");
    if(!source)
        return std::nullopt;
    calibration<double> out{};
    double value = 0.0;
    if(source.attribute("rising") && read_finite(source.attribute("rising").value(), value, ctx,
                                                 loc, "rising"))
        out.rising = value;
    if(source.attribute("falling") && read_finite(source.attribute("falling").value(), value, ctx,
                                                  loc, "falling"))
        out.falling = value;
    return out;
}

}

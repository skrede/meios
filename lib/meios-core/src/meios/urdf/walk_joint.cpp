#include "urdf_detail.h"

#include "meios/records/joint.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

constexpr std::string_view declared_kinds =
    "revolute, continuous, prismatic, fixed, floating, planar";

std::optional<joint_kind> kind_from_string(std::string_view text)
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
    if(text == "fixed")
        return joint_kind::fixed;
    return std::nullopt;
}

// An absent type and a misspelled one are different author mistakes, so they carry different
// codes and only the second can name the set the author missed.
bool resolve_kind(pugi::xml_node node, joint_kind &out, parse_context &ctx,
                  const source_location &loc)
{
    bool ok = true;
    const std::string owner = node.attribute("name").value();
    const pugi::xml_attribute declared = node.attribute("type");
    if(!declared)
    {
        report_structural(ctx, loc, diagnostic_code::missing_joint_type,
                          "joint '" + owner + "' declares no type", ok);
        return false;
    }
    const std::optional<joint_kind> kind = kind_from_string(declared.value());
    if(!kind)
        report_structural(ctx, loc, diagnostic_code::unknown_joint_type,
                          "joint '" + owner + "' declares unrecognized type '"
                              + declared.value() + "'; the declared types are "
                              + std::string(declared_kinds),
                          ok);
    else
        out = *kind;
    return ok;
}

bool requires_limit(joint_kind kind)
{
    return kind == joint_kind::revolute || kind == joint_kind::prismatic;
}

bool read_bound(pugi::xml_node limit, const char *field, double &out, parse_context &ctx,
                const source_location &loc)
{
    const pugi::xml_attribute declared = limit.attribute(field);
    if(declared)
        return read_finite(declared.value(), out, ctx, loc, field);
    bool ok = true;
    report_structural(ctx, loc, diagnostic_code::missing_required_field,
                      std::string("<limit> declares no '") + field + "'", ok);
    return ok;
}

std::optional<joint_limits<double>> read_limits(pugi::xml_node node, parse_context &ctx,
                                                const source_location &loc)
{
    const pugi::xml_node limit = node.child("limit");
    if(!limit)
        return std::nullopt;
    joint_limits<double> out{};
    bool ok = read_optional(limit, "lower", out.lower, ctx, loc);
    ok = read_optional(limit, "upper", out.upper, ctx, loc) && ok;
    ok = read_bound(limit, "effort", out.effort, ctx, loc) && ok;
    ok = read_bound(limit, "velocity", out.velocity, ctx, loc) && ok;
    if(!ok)
        return std::nullopt;
    return out;
}

vector3<double> read_axis(pugi::xml_node node, parse_context &ctx, const source_location &loc)
{
    const pugi::xml_node axis = node.child("axis");
    if(!axis)
        return vector3<double>{ 1.0, 0.0, 0.0 };
    return read_vec3(axis.attribute("xyz").value(), ctx, loc, "axis");
}

// A joint is structural and is never dropped, so an unreadable <origin> is refused under
// the strict setting and disclosed under the permissive ones; there is no element to drop.
void read_detail(pugi::xml_node node, joint<double> &out, parse_context &ctx,
                 const source_location &loc)
{
    read_transform(node.child("origin"), out.origin, ctx, loc);
    out.axis = read_axis(node, ctx, loc);
    out.limits = read_limits(node, ctx, loc);
    out.couple = read_mimic(node, ctx, loc);
    out.dyn = read_dynamics(node, ctx, loc);
    out.safety = read_safety(node, ctx, loc);
    out.calib = read_calibration(node, ctx, loc);
}

std::optional<joint<double>> refuse_unbounded(const std::string &owner, parse_context &ctx,
                                              const source_location &loc)
{
    bool ok = true;
    report_structural(ctx, loc, diagnostic_code::missing_limit,
                      "joint '" + owner + "' is of a bounded kind and declares no usable <limit>",
                      ok);
    return std::nullopt;
}

}

std::optional<joint<double>> extract_joint(pugi::xml_node node, std::string_view text,
                                           const std::filesystem::path &file, parse_context &ctx)
{
    const source_location loc = node_location(node, text, file);
    joint<double> out{};
    out.name = node.attribute("name").value();
    if(!resolve_kind(node, out.kind, ctx, loc))
        return std::nullopt;
    out.parent = node.child("parent").attribute("link").value();
    out.child = node.child("child").attribute("link").value();
    read_detail(node, out, ctx, loc);
    out.closes_loop = false;
    out.origin_loc = loc;
    if(requires_limit(out.kind) && !out.limits)
        return refuse_unbounded(out.name, ctx, loc);
    return out;
}

}

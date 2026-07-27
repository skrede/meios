#include "urdf_detail.h"

#include "meios/records/link.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <optional>

namespace meios::detail
{

namespace
{

void report_absent(parse_context &ctx, const source_location &loc, const char *part)
{
    report_drop(ctx, loc, diagnostic_code::missing_required_field,
                std::string("<inertial> declares no <") + part
                    + ">, so the inertial is dropped rather than completed with a zero");
}

bool read_mass(pugi::xml_node node, double &out, parse_context &ctx, const source_location &loc)
{
    const pugi::xml_node mass = node.child("mass");
    if(!mass)
    {
        report_absent(ctx, loc, "mass");
        return false;
    }
    return read_required(mass, "value", out, ctx, loc);
}

bool read_tensor(pugi::xml_node node, inertia<double> &out, parse_context &ctx,
                 const source_location &loc)
{
    const pugi::xml_node source = node.child("inertia");
    if(!source)
    {
        report_absent(ctx, loc, "inertia");
        return false;
    }
    bool ok = read_required(source, "ixx", out.ixx, ctx, loc);
    ok = read_required(source, "ixy", out.ixy, ctx, loc) && ok;
    ok = read_required(source, "ixz", out.ixz, ctx, loc) && ok;
    ok = read_required(source, "iyy", out.iyy, ctx, loc) && ok;
    ok = read_required(source, "iyz", out.iyz, ctx, loc) && ok;
    return read_required(source, "izz", out.izz, ctx, loc) && ok;
}

}

std::optional<inertial<double>> read_inertial(pugi::xml_node node, parse_context &ctx,
                                              const source_location &loc)
{
    inertial<double> out{};
    bool ok = read_transform(node.child("origin"), out.origin, ctx, loc);
    ok = read_mass(node, out.mass, ctx, loc) && ok;
    ok = read_tensor(node, out.tensor, ctx, loc) && ok;
    if(!ok)
        return std::nullopt;
    return out;
}

}

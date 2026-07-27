#include "urdf_detail.h"

#include "meios/records/link.h"

#include "meios/xacro/detail/numeric.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <cmath>
#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

rgba<double> read_rgba(std::string_view text, parse_context &ctx, const source_location &loc)
{
    double v[4] = { 0.0, 0.0, 0.0, 1.0 };
    read_scalars(text, v, 4, ctx, loc, "rgba");
    return rgba<double>{ v[0], v[1], v[2], v[3] };
}

std::optional<std::string> resolve_material_ref(pugi::xml_node node, const material_table &table,
                                                parse_context &ctx, const source_location &loc)
{
    const std::string name = node.attribute("name").value();
    if(node.child("color") || node.child("texture") || table.count(name) != 0)
        return name;
    if(ctx.materials != material_policy::skip)
    {
        const level lvl = ctx.materials == material_policy::fail ? level::error : level::warn;
        ctx.log.log(lvl, diagnostic_code::undefined_material, loc,
                    "visual references undefined material '" + name + "'");
    }
    return std::nullopt;
}

inertial<double> read_inertial(pugi::xml_node node, std::string_view text,
                               const std::filesystem::path &file, parse_context &ctx)
{
    const source_location loc = node_location(node, text, file);
    inertial<double> out{};
    out.origin = read_transform(node.child("origin"), ctx, loc);
    read_finite(node.child("mass").attribute("value").value(), out.mass, ctx, loc, "mass");
    const pugi::xml_node i = node.child("inertia");
    read_finite(i.attribute("ixx").value(), out.tensor.ixx, ctx, loc, "ixx");
    read_finite(i.attribute("ixy").value(), out.tensor.ixy, ctx, loc, "ixy");
    read_finite(i.attribute("ixz").value(), out.tensor.ixz, ctx, loc, "ixz");
    read_finite(i.attribute("iyy").value(), out.tensor.iyy, ctx, loc, "iyy");
    read_finite(i.attribute("iyz").value(), out.tensor.iyz, ctx, loc, "iyz");
    read_finite(i.attribute("izz").value(), out.tensor.izz, ctx, loc, "izz");
    return out;
}

visual<double> read_visual(pugi::xml_node node, std::string_view text,
                           const std::filesystem::path &file, parse_context &ctx,
                           const material_table &materials)
{
    const source_location loc = node_location(node, text, file);
    visual<double> out{};
    out.origin = read_transform(node.child("origin"), ctx, loc);
    out.geom = read_geometry(node.child("geometry"), ctx, loc);
    if(pugi::xml_node mat = node.child("material"))
    {
        if(mat.child("color") || mat.child("texture"))
            out.material_inline = extract_material(mat, text, file, ctx);
        else
            out.material_ref = resolve_material_ref(mat, materials, ctx, node_location(mat, text, file));
    }
    return out;
}

collision<double> read_collision(pugi::xml_node node, std::string_view text,
                                 const std::filesystem::path &file, parse_context &ctx)
{
    const source_location loc = node_location(node, text, file);
    collision<double> out{};
    out.origin = read_transform(node.child("origin"), ctx, loc);
    out.geom = read_geometry(node.child("geometry"), ctx, loc);
    return out;
}

}

bool read_finite(std::string_view text, double &out, parse_context &ctx,
                 const source_location &loc, std::string_view field)
{
    bool ok = false;
    const double value = parse_double(text, ok);
    if(!ok)
        return false;
    if(!std::isfinite(value))
    {
        ctx.log.log(level::error, loc, "non-finite value for '" + std::string(field) + "'");
        out = 0.0;
        return false;
    }
    out = value;
    return true;
}

vector3<double> read_vec3(std::string_view text, parse_context &ctx, const source_location &loc,
                          std::string_view field)
{
    double v[3] = { 0.0, 0.0, 0.0 };
    read_scalars(text, v, 3, ctx, loc, field);
    return vector3<double>{ v[0], v[1], v[2] };
}

transform<double> read_transform(pugi::xml_node origin, parse_context &ctx,
                                 const source_location &loc)
{
    transform<double> out{};
    out.translation = read_vec3(origin.attribute("xyz").value(), ctx, loc, "xyz");
    const vector3<double> angles = read_vec3(origin.attribute("rpy").value(), ctx, loc, "rpy");
    out.rotation = rpy<double>{ angles.x, angles.y, angles.z };
    return out;
}

material<double> extract_material(pugi::xml_node node, std::string_view text,
                                  const std::filesystem::path &file, parse_context &ctx)
{
    const source_location loc = node_location(node, text, file);
    material<double> out{};
    out.name = node.attribute("name").value();
    if(pugi::xml_node color = node.child("color"))
        out.color = read_rgba(color.attribute("rgba").value(), ctx, loc);
    if(pugi::xml_node texture = node.child("texture"))
        out.texture = std::string(texture.attribute("filename").value());
    return out;
}

link<double> extract_link(pugi::xml_node node, std::string_view text,
                          const std::filesystem::path &file, parse_context &ctx,
                          const material_table &materials)
{
    link<double> out{};
    out.name = node.attribute("name").value();
    out.origin_loc = node_location(node, text, file);
    if(pugi::xml_node body = node.child("inertial"))
        out.body = read_inertial(body, text, file, ctx);
    for(pugi::xml_node visual : node.children("visual"))
        out.visuals.push_back(read_visual(visual, text, file, ctx, materials));
    for(pugi::xml_node collision : node.children("collision"))
        out.collisions.push_back(read_collision(collision, text, file, ctx));
    return out;
}

}

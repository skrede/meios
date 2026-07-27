#include "urdf_detail.h"

#include "meios/records/link.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

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

void attach_material(pugi::xml_node node, visual<double> &out, std::string_view text,
                     const std::filesystem::path &file, parse_context &ctx,
                     const material_table &materials)
{
    pugi::xml_node mat = node.child("material");
    if(!mat)
        return;
    if(mat.child("color") || mat.child("texture"))
        out.material_inline = extract_material(mat, text, file, ctx);
    else
        out.material_ref = resolve_material_ref(mat, materials, ctx, node_location(mat, text, file));
}

std::optional<visual<double>> read_visual(pugi::xml_node node, std::string_view text,
                                          const std::filesystem::path &file, parse_context &ctx,
                                          const material_table &materials)
{
    const source_location loc = node_location(node, text, file);
    visual<double> out{};
    if(!read_transform(node.child("origin"), out.origin, ctx, loc))
        return std::nullopt;
    const std::optional<geometry<double>> shape =
        read_geometry(node.child("geometry"), "visual", ctx, loc);
    if(!shape)
        return std::nullopt;
    out.geom = *shape;
    attach_material(node, out, text, file, ctx, materials);
    return out;
}

std::optional<collision<double>> read_collision(pugi::xml_node node, std::string_view text,
                                                const std::filesystem::path &file,
                                                parse_context &ctx)
{
    const source_location loc = node_location(node, text, file);
    collision<double> out{};
    if(!read_transform(node.child("origin"), out.origin, ctx, loc))
        return std::nullopt;
    const std::optional<geometry<double>> shape =
        read_geometry(node.child("geometry"), "collision", ctx, loc);
    if(!shape)
        return std::nullopt;
    out.geom = *shape;
    return out;
}

}

vector3<double> read_vec3(std::string_view text, parse_context &ctx, const source_location &loc,
                          std::string_view field)
{
    double v[3] = { 0.0, 0.0, 0.0 };
    read_scalars(text, v, 3, ctx, loc, field);
    return vector3<double>{ v[0], v[1], v[2] };
}

bool read_transform(pugi::xml_node origin, transform<double> &out, parse_context &ctx,
                    const source_location &loc)
{
    double offset[3] = { 0.0, 0.0, 0.0 };
    double angles[3] = { 0.0, 0.0, 0.0 };
    const bool moved = read_scalars(origin.attribute("xyz").value(), offset, 3, ctx, loc, "xyz");
    const bool turned = read_scalars(origin.attribute("rpy").value(), angles, 3, ctx, loc, "rpy");
    out.translation = vector3<double>{ offset[0], offset[1], offset[2] };
    out.rotation = rpy<double>{ angles[0], angles[1], angles[2] };
    return moved && turned;
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
        out.body = read_inertial(body, ctx, node_location(body, text, file));
    for(pugi::xml_node vis : node.children("visual"))
        if(std::optional<visual<double>> built = read_visual(vis, text, file, ctx, materials))
            out.visuals.push_back(*built);
    for(pugi::xml_node col : node.children("collision"))
        if(std::optional<collision<double>> built = read_collision(col, text, file, ctx))
            out.collisions.push_back(*built);
    return out;
}

}

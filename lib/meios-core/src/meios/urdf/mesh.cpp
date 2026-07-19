#include "lfs_pointer.h"
#include "urdf_detail.h"

#include "meios/records/geometry.h"

#include "meios/io/materialize.h"
#include "meios/io/source_stack.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

cylinder<double> read_cylinder(pugi::xml_node node, parse_context &ctx, const source_location &loc)
{
    cylinder<double> out{};
    read_finite(node.attribute("radius").value(), out.radius, ctx, loc, "radius");
    read_finite(node.attribute("length").value(), out.length, ctx, loc, "length");
    return out;
}

sphere<double> read_sphere(pugi::xml_node node, parse_context &ctx, const source_location &loc)
{
    sphere<double> out{};
    read_finite(node.attribute("radius").value(), out.radius, ctx, loc, "radius");
    return out;
}

void report_missing(parse_context &ctx, const source_location &loc, const std::string &uri)
{
    if(ctx.on_missing == missing_asset::skip)
        return;
    const level lvl = ctx.on_missing == missing_asset::fail ? level::error : level::warn;
    ctx.log.log(lvl, diagnostic_code::unresolved_mesh, loc, "could not resolve mesh '" + uri + "'");
}

void record_resolved(mesh<double> &shape, resolved_asset &&asset, parse_context &ctx)
{
    if(asset.holds_path())
    {
        shape.resolved_path = asset.path().string();
        return;
    }
    resolved_asset local = materialize(std::move(asset), ctx.log);
    shape.resolved_path = local.path().string();
}

mesh<double> read_mesh(pugi::xml_node node, parse_context &ctx, const source_location &loc)
{
    mesh<double> out{};
    out.filename = node.attribute("filename").value();
    out.scale = node.attribute("scale") ? read_vec3(node.attribute("scale").value(), ctx, loc, "scale")
                                        : vector3<double>{ 1.0, 1.0, 1.0 };
    resolve_mesh(out, ctx, loc);
    return out;
}

}

void resolve_mesh(mesh<double> &shape, parse_context &ctx, const source_location &loc)
{
    constexpr std::string_view prefix = "package://";
    std::string_view uri = shape.filename;
    if(!uri.starts_with(prefix))
        return;
    uri.remove_prefix(prefix.size());
    const std::size_t slash = uri.find('/');
    if(slash == std::string_view::npos)
    {
        ctx.log.log(level::error, diagnostic_code::malformed_mesh_uri, loc,
                    "malformed package:// mesh URI '" + shape.filename + "'");
        return;
    }
    const std::string pkg(uri.substr(0, slash));
    const std::string rel(uri.substr(slash + 1));
    std::optional<resolved_asset> hit = ctx.sources.locate(pkg, rel, ctx.log);
    if(!hit)
    {
        report_missing(ctx, loc, shape.filename);
        return;
    }
    record_resolved(shape, std::move(*hit), ctx);
    if(shape.resolved_path && is_lfs_pointer(*shape.resolved_path))
    {
        ctx.log.log(level::error, diagnostic_code::lfs_pointer_mesh, loc,
                    "resolved mesh '" + shape.filename + "' is an unsmudged Git-LFS pointer, not geometry");
        shape.resolved_path.reset();
    }
}

geometry<double> read_geometry(pugi::xml_node node, parse_context &ctx, const source_location &loc)
{
    geometry<double> out{};
    if(pugi::xml_node box_node = node.child("box"))
        out.shape = box<double>{ read_vec3(box_node.attribute("size").value(), ctx, loc, "size") };
    else if(pugi::xml_node cylinder_node = node.child("cylinder"))
        out.shape = read_cylinder(cylinder_node, ctx, loc);
    else if(pugi::xml_node sphere_node = node.child("sphere"))
        out.shape = read_sphere(sphere_node, ctx, loc);
    else if(pugi::xml_node mesh_node = node.child("mesh"))
        out.shape = read_mesh(mesh_node, ctx, loc);
    return out;
}

}

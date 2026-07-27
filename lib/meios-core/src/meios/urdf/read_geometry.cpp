#include "urdf_detail.h"

#include "meios/records/geometry.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

template <typename Shape>
std::optional<geometry<double>> as_geometry(std::optional<Shape> shape)
{
    if(!shape)
        return std::nullopt;
    return geometry<double>{ std::move(*shape) };
}

std::optional<box<double>> read_box(pugi::xml_node node, parse_context &ctx,
                                    const source_location &loc)
{
    const pugi::xml_attribute size = node.attribute("size");
    if(!size)
    {
        report_drop(ctx, loc, diagnostic_code::missing_required_field, "<box> declares no 'size'");
        return std::nullopt;
    }
    double edges[3] = { 0.0, 0.0, 0.0 };
    if(!read_scalars(size.value(), edges, 3, ctx, loc, "size"))
        return std::nullopt;
    return box<double>{ vector3<double>{ edges[0], edges[1], edges[2] } };
}

std::optional<cylinder<double>> read_cylinder(pugi::xml_node node, parse_context &ctx,
                                              const source_location &loc)
{
    cylinder<double> out{};
    bool ok = read_required(node, "radius", out.radius, ctx, loc);
    ok = read_required(node, "length", out.length, ctx, loc) && ok;
    if(!ok)
        return std::nullopt;
    return out;
}

std::optional<sphere<double>> read_sphere(pugi::xml_node node, parse_context &ctx,
                                          const source_location &loc)
{
    sphere<double> out{};
    if(!read_required(node, "radius", out.radius, ctx, loc))
        return std::nullopt;
    return out;
}

std::optional<mesh<double>> read_mesh(pugi::xml_node node, parse_context &ctx,
                                      const source_location &loc)
{
    const pugi::xml_attribute filename = node.attribute("filename");
    if(!filename)
    {
        report_drop(ctx, loc, diagnostic_code::missing_required_field,
                    "<mesh> declares no 'filename'");
        return std::nullopt;
    }
    mesh<double> out{};
    out.filename = filename.value();
    out.scale = node.attribute("scale")
                    ? read_vec3(node.attribute("scale").value(), ctx, loc, "scale")
                    : vector3<double>{ 1.0, 1.0, 1.0 };
    resolve_mesh(out, ctx, loc);
    return out;
}

pugi::xml_node first_element(pugi::xml_node node)
{
    for(pugi::xml_node child : node.children())
        if(child.type() == pugi::node_element)
            return child;
    return {};
}

std::optional<geometry<double>> refuse_shape(pugi::xml_node node, parse_context &ctx,
                                             const source_location &loc)
{
    const pugi::xml_node child = first_element(node);
    if(!child)
        report_drop(ctx, loc, diagnostic_code::missing_geometry, "<geometry> declares no shape");
    else
        report_drop(ctx, loc, diagnostic_code::unknown_geometry_shape,
                    "<geometry> declares unrecognized shape <" + std::string(child.name()) + ">");
    return std::nullopt;
}

}

std::optional<geometry<double>> read_geometry(pugi::xml_node node, std::string_view owner,
                                              parse_context &ctx, const source_location &loc)
{
    if(!node)
    {
        report_drop(ctx, loc, diagnostic_code::missing_geometry,
                    "<" + std::string(owner) + "> declares no <geometry>");
        return std::nullopt;
    }
    if(pugi::xml_node shape = node.child("box"))
        return as_geometry(read_box(shape, ctx, loc));
    if(pugi::xml_node shape = node.child("cylinder"))
        return as_geometry(read_cylinder(shape, ctx, loc));
    if(pugi::xml_node shape = node.child("sphere"))
        return as_geometry(read_sphere(shape, ctx, loc));
    if(pugi::xml_node shape = node.child("mesh"))
        return as_geometry(read_mesh(shape, ctx, loc));
    return refuse_shape(node, ctx, loc);
}

}

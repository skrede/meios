#include "urdf_detail.h"

#include "meios/urdf/urdf_reader.h"

#include "meios/records/robot_info.h"

#include "meios/model/tree.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace
{

pugi::xml_node robot_root(pugi::xml_node document)
{
    for(pugi::xml_node child : document.children())
        if(child.type() == pugi::node_element)
            return child;
    return {};
}

template <typename Sink>
void emit_materials(pugi::xml_node robot, std::string_view text, const std::filesystem::path &file,
                    parse_context &ctx, detail::material_table &table, Sink &sink)
{
    for(pugi::xml_node node : robot.children("material"))
    {
        material<double> mat = detail::extract_material(node, text, file, ctx);
        table.emplace(mat.name, mat);
        sink.on_material(mat);
    }
}

template <typename Sink>
void lift_inline_materials(link<double> &built, detail::material_table &table, Sink &sink)
{
    for(visual<double> &vis : built.visuals)
    {
        if(!vis.material_inline || vis.material_inline->name.empty())
            continue;
        if(table.emplace(vis.material_inline->name, *vis.material_inline).second)
            sink.on_material(*vis.material_inline);
        vis.material_ref = vis.material_inline->name;
        vis.material_inline.reset();
    }
}

template <typename Sink>
void emit_links(pugi::xml_node robot, std::string_view text, const std::filesystem::path &file,
                parse_context &ctx, detail::material_table &table, Sink &sink)
{
    for(pugi::xml_node node : robot.children("link"))
    {
        link<double> built = detail::extract_link(node, text, file, ctx, table);
        lift_inline_materials(built, table, sink);
        sink.on_link(built);
    }
}

std::optional<std::string> declared_version(pugi::xml_node robot)
{
    if(pugi::xml_attribute version = robot.attribute("version"))
        return std::string(version.value());
    return std::nullopt;
}

template <typename Sink>
void emit_joints(pugi::xml_node robot, std::string_view text, const std::filesystem::path &file,
                 parse_context &ctx, Sink &sink)
{
    for(pugi::xml_node node : robot.children("joint"))
        sink.on_joint(detail::extract_joint(node, text, file, ctx));
}

}

void basic_parser<urdf_reader>::parse_erased(std::string_view source, erased_sink &sink)
{
    pugi::xml_document doc;
    const unsigned flags = pugi::parse_default | pugi::parse_comments | pugi::parse_ws_pcdata;
    const pugi::xml_parse_result parsed = doc.load_buffer(source.data(), source.size(), flags);
    if(!parsed)
    {
        m_context.log.log(level::error, diagnostic_code::xml_parse_error,
                          detail::offset_location(source, parsed.offset, m_context.document),
                          std::string("urdf parse error: ") + parsed.description());
        return;
    }
    const std::filesystem::path &file = m_context.document;
    if(!detail::run_strictness(doc, source, file, m_context) && m_context.strict == strictness::fail)
        return;
    const pugi::xml_node robot = robot_root(doc);
    if(!detail::check_identity(robot, source, file, m_context))
        return;
    sink.on_robot(
        robot_info{ std::string(robot.attribute("name").value()), {}, declared_version(robot) });
    detail::material_table table;
    emit_materials(robot, source, file, m_context, table, sink);
    emit_links(robot, source, file, m_context, table, sink);
    emit_joints(robot, source, file, m_context, sink);
    sink.finish();
}

}

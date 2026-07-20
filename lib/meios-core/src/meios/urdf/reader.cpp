#include "urdf_detail.h"

#include "meios/urdf/urdf_reader.h"

#include "meios/records/robot_info.h"

#include "meios/model/tree.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
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

bool is_handled_child(std::string_view tag)
{
    return tag == "material" || tag == "link" || tag == "joint";
}

bool is_known_extension(std::string_view tag)
{
    return tag == "gazebo" || tag == "ros2_control" || tag == "transmission" || tag == "sensor";
}

void warn_unhandled(pugi::xml_node robot, std::string_view text, const std::filesystem::path &file,
                    parse_context &ctx)
{
    for(pugi::xml_node child : robot.children())
    {
        if(child.type() != pugi::node_element || is_handled_child(child.name()))
            continue;
        std::string named = child.name();
        if(pugi::xml_attribute name = child.attribute("name"))
            named += " name='" + std::string(name.value()) + '\'';
        const std::string kind = is_known_extension(child.name()) ? "extension " : "";
        ctx.log.log(level::warn, detail::node_location(child, text, file),
                    "dropping unhandled robot-level " + kind + "element <" + named + '>');
    }
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
    if(!detail::run_strictness(doc, source, file, m_context) && m_context.strict == strictness::strict)
        return;
    const pugi::xml_node robot = robot_root(doc);
    sink.on_robot(robot_info{ std::string(robot.attribute("name").value()), {} });
    detail::material_table table;
    emit_materials(robot, source, file, m_context, table, sink);
    emit_links(robot, source, file, m_context, table, sink);
    emit_joints(robot, source, file, m_context, sink);
    warn_unhandled(robot, source, file, m_context);
    sink.finish();
}

}

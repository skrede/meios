#include "urdf_detail.h"

#include "meios/urdf/urdf_reader.h"

#include "meios/bundle/urdf_writer.h"

#include "meios/sink/pod_recorder.h"
#include "meios/sink/world_recorder.h"

#include "meios/records/robot_info.h"

#include "meios/model/tree.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

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
void emit_links(pugi::xml_node robot, std::string_view text, const std::filesystem::path &file,
                parse_context &ctx, const detail::material_table &table, Sink &sink)
{
    for(pugi::xml_node node : robot.children("link"))
        sink.on_link(detail::extract_link(node, text, file, ctx, table));
}

template <typename Sink>
void emit_joints(pugi::xml_node robot, std::string_view text, const std::filesystem::path &file,
                 parse_context &ctx, Sink &sink)
{
    for(pugi::xml_node node : robot.children("joint"))
        sink.on_joint(detail::extract_joint(node, text, file, ctx));
}

}

template <typename Sink>
    requires model_sink<Sink>
void basic_parser<urdf_reader>::parse(std::string_view source, Sink &sink)
{
    pugi::xml_document doc;
    const unsigned flags = pugi::parse_default | pugi::parse_comments | pugi::parse_ws_pcdata;
    const pugi::xml_parse_result parsed = doc.load_buffer(source.data(), source.size(), flags);
    if(!parsed)
    {
        m_context.log.log(level::error, std::string("urdf parse error: ") + parsed.description());
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
    sink.finish();
}

template void basic_parser<urdf_reader>::parse<pod_recorder<tree<double>>>(
    std::string_view, pod_recorder<tree<double>> &);
template void basic_parser<urdf_reader>::parse<world_recorder>(std::string_view, world_recorder &);
template void basic_parser<urdf_reader>::parse<urdf_writer>(std::string_view, urdf_writer &);

}

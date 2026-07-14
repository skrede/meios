#include "writer_detail.h"

#include "meios/bundle/urdf_writer.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <pugixml.hpp>

#include <map>
#include <string>
#include <memory>
#include <utility>

namespace meios
{

struct urdf_writer::document
{
    pugi::xml_document doc;
    pugi::xml_node root;
};

namespace
{

void rewrite_node(pugi::xml_node node, const std::map<std::string, std::string> &remap)
{
    if(pugi::xml_attribute file = node.attribute("filename"))
    {
        const auto hit = remap.find(file.value());
        if(hit != remap.end())
            file.set_value(hit->second.c_str());
    }
    for(pugi::xml_node child : node.children())
        rewrite_node(child, remap);
}

bool node_chars_ok(pugi::xml_node node, std::string &offender)
{
    for(pugi::xml_attribute attr : node.attributes())
        if(!detail::xml10_char_ok(attr.value()))
        {
            offender = attr.name();
            return false;
        }
    for(pugi::xml_node child : node.children())
        if(!node_chars_ok(child, offender))
            return false;
    return true;
}

}

urdf_writer::urdf_writer(std::ostream &out, log_sink &log)
    : m_out(out), m_log(log), m_bundle_name(), m_result{ emit_status::ok, 0, 0 },
      m_doc(std::make_unique<document>()), m_refs(), m_plan_cb()
{}

urdf_writer::urdf_writer(std::ostream &out, log_sink &log, std::string bundle_name,
                         reference_planner plan_cb)
    : m_out(out), m_log(log), m_bundle_name(std::move(bundle_name)),
      m_result{ emit_status::ok, 0, 0 }, m_doc(std::make_unique<document>()), m_refs(),
      m_plan_cb(std::move(plan_cb))
{}

urdf_writer::~urdf_writer() = default;

void urdf_writer::warn_empty(const std::string &name)
{
    if(name.empty())
        m_log.log(level::warn, "emitting a record with an empty name");
}

void urdf_writer::on_robot(const robot_info &robot)
{
    warn_empty(robot.name);
    m_doc->root = m_doc->doc.append_child("robot");
    m_doc->root.append_attribute("name").set_value(robot.name.c_str());
}

void urdf_writer::on_material(const material<double> &mat)
{
    warn_empty(mat.name);
    detail::append_material(m_doc->root, mat, m_refs);
}

void urdf_writer::on_link(const link<double> &node)
{
    warn_empty(node.name);
    detail::append_link(m_doc->root, node, m_refs);
}

void urdf_writer::on_joint(const joint<double> &edge)
{
    warn_empty(edge.name);
    detail::append_joint(m_doc->root, edge);
}

void urdf_writer::apply_rewrite()
{
    if(!m_plan_cb.valid())
        return;
    const std::map<std::string, std::string> remap = m_plan_cb(m_refs);
    rewrite_node(m_doc->root, remap);
}

void urdf_writer::finish()
{
    apply_rewrite();
    m_result.assets_seen = m_refs.size();
    std::string offender;
    if(!node_chars_ok(m_doc->root, offender))
    {
        m_log.log(level::error, source_location{ m_bundle_name, 0, 0 },
                  "attribute '" + offender + "' holds a character XML 1.0 cannot encode");
        m_result.status = emit_status::xml_unencodable;
        return;
    }
    m_doc->doc.save(m_out, "  ");
    m_result.status = emit_status::ok;
}

const emit_result &urdf_writer::status() const
{
    return m_result;
}

}

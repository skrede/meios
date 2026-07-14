#include "tree.h"
#include "label_escape.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"

#include <string>
#include <sstream>
#include <ostream>
#include <string_view>

namespace meios::cli
{

namespace
{

bool is_movable(joint_kind kind)
{
    return kind == joint_kind::revolute || kind == joint_kind::prismatic
        || kind == joint_kind::continuous;
}

void write_edge(std::ostream &out, const joint<double> &edge)
{
    const std::string_view style = edge.closes_loop ? "dotted" : (is_movable(edge.kind) ? "solid" : "dashed");
    out << "  \"" << escape_dot(edge.parent) << "\" -> \"" << escape_dot(edge.child)
        << "\" [label=\"" << escape_dot(edge.name) << ": " << joint_kind_label(edge.kind)
        << "\", style=" << style << (edge.closes_loop ? ", color=red" : "") << "];\n";
}

}

std::string render_dot(const model<double> &robot)
{
    std::ostringstream out;
    out << "digraph robot {\n";
    for(const link<double> &node : robot.links)
        out << "  \"" << escape_dot(node.name) << "\";\n";
    for(const joint<double> &edge : robot.joints)
        write_edge(out, edge);
    out << "}\n";
    return out.str();
}

}

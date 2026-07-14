#include "tree.h"
#include "label_escape.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"

#include <string>
#include <vector>
#include <cstddef>
#include <sstream>
#include <ostream>

namespace meios::cli
{

namespace
{

void render_edge(std::ostream &out, const model<double> &robot, const child_map &children,
                 const tree_edge &edge, const std::string &prefix, bool last);

void render_subtree(std::ostream &out, const model<double> &robot, const child_map &children,
                    std::size_t link, const std::string &prefix)
{
    const std::vector<tree_edge> &edges = children[link];
    for(std::size_t i = 0; i < edges.size(); ++i)
        render_edge(out, robot, children, edges[i], prefix, i + 1 == edges.size());
}

void render_edge(std::ostream &out, const model<double> &robot, const child_map &children,
                 const tree_edge &edge, const std::string &prefix, bool last)
{
    const joint<double> &link_edge = robot.joints[edge.joint];
    out << prefix << (last ? "└─ " : "├─ ")
        << '[' << escape_ascii(link_edge.name) << ": " << joint_kind_label(link_edge.kind) << "] "
        << escape_ascii(robot.links[edge.child].name) << '\n';
    render_subtree(out, robot, children, edge.child, prefix + (last ? "   " : "│  "));
}

}

std::string render_ascii(const model<double> &robot, const child_map &children,
                         const std::vector<std::size_t> &roots)
{
    std::ostringstream out;
    if(roots.size() > 1)
        out << "# " << roots.size() << " roots\n";
    for(const std::size_t root : roots)
    {
        out << escape_ascii(robot.links[root].name) << '\n';
        render_subtree(out, robot, children, root, "");
    }
    return out.str();
}

}

#ifndef HPP_GUARD_MEIOS_CLI_TREE_H
#define HPP_GUARD_MEIOS_CLI_TREE_H

#include "meios/model/model.h"

#include "meios/records/joint.h"

#include <string>
#include <vector>
#include <cstddef>
#include <string_view>

namespace meios::cli
{

// A tree edge from a parent link to one child: the child link index and the joint
// index that connects them, so the renderer can annotate the edge with joint data.
struct tree_edge
{
    std::size_t child;
    std::size_t joint;
};

using child_map = std::vector<std::vector<tree_edge>>;

std::string_view joint_kind_label(joint_kind kind);

// Inverts parent_of into a per-link child edge list, pairing each child with the
// joint that made it that parent's child (loop-closure joints are not tree edges).
child_map build_children(const model<double> &robot, const std::vector<int> &parent_of);

std::vector<std::size_t> roots_of(const std::vector<int> &parent_of);

std::string render_ascii(const model<double> &robot, const child_map &children,
                         const std::vector<std::size_t> &roots);

std::string render_dot(const model<double> &robot);

}

#endif

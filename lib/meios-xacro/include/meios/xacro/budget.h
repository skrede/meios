#ifndef HPP_GUARD_MEIOS_XACRO_BUDGET_H
#define HPP_GUARD_MEIOS_XACRO_BUDGET_H

#include <cstddef>

namespace meios
{

// One recursive entry into the expansion walk is one node processed, and a level of it costs
// just under a kilobyte of native stack -- 998 bytes under gcc 16.2 at -O0 and 1 004 under
// clang 22.1 at -O2, measured by driving a self-instantiating macro, a nesting of conditionals
// and a nesting of plain elements to a ceiling until the process died. The stack was exhausted
// at 522 levels on a 512 KiB stack, which is the smallest a consumer plausibly runs this on
// (a secondary thread's default on macOS), and at 8 583 on the 8 MiB Linux default. This admits
// 128 levels, some 128 KiB, against a deepest measured description of 11 -- the deepest of the
// 17 pinned corpus entry points, five Franka arms. The expression-depth ceiling claims its own
// share of the same stack at the deepest level reached, so the two are read together.
inline constexpr std::size_t default_expansion_depth_ceiling = 128;

// Three independent ceilings bound expansion: a work count (nodes visited, macro
// instantiations, substitutions), an emitted-node count, and the depth of the walk.
// Depth alone would miss a shallow-but-wide macro fan-out and the two volume counts
// alone miss a recursion that takes the native stack thousands of levels before a
// million work units accrue, so the three are tracked separately; any one crossing
// its ceiling halts expansion with a resource diagnostic.
struct expansion_limits
{
    expansion_limits(std::size_t work_ceiling = 1'000'000,
                     std::size_t node_ceiling = 100'000,
                     std::size_t depth_ceiling = default_expansion_depth_ceiling)
        : work(work_ceiling), output_nodes(node_ceiling), depth(depth_ceiling)
    {
    }

    std::size_t work;
    std::size_t output_nodes;
    std::size_t depth;
};

// Depth is the high-water mark of the walk's live recursion rather than a running total:
// reaching a level is what the ceiling bounds, not how many times it is reached.
struct expansion_counters
{
    expansion_counters() : work(0), output_nodes(0), depth(0) {}

    std::size_t work;
    std::size_t output_nodes;
    std::size_t depth;
};

}

#endif

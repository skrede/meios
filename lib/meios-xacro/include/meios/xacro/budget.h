#ifndef HPP_GUARD_MEIOS_XACRO_BUDGET_H
#define HPP_GUARD_MEIOS_XACRO_BUDGET_H

#include <cstddef>

namespace meios
{

// Two independent ceilings bound expansion: a work count (nodes visited, macro
// instantiations, substitutions) and an emitted-node count. Depth alone would
// miss a shallow-but-wide macro fan-out, so the two are tracked separately;
// either crossing its ceiling halts expansion with a resource diagnostic.
struct expansion_limits
{
    expansion_limits(std::size_t work_ceiling = 1'000'000,
                     std::size_t node_ceiling = 100'000)
        : work(work_ceiling), output_nodes(node_ceiling)
    {
    }

    std::size_t work;
    std::size_t output_nodes;
};

struct expansion_counters
{
    expansion_counters() : work(0), output_nodes(0) {}

    std::size_t work;
    std::size_t output_nodes;
};

}

#endif

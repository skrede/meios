#ifndef HPP_GUARD_MEIOS_XACRO_EVALUATOR_LIMITS_H
#define HPP_GUARD_MEIOS_XACRO_EVALUATOR_LIMITS_H

#include <cstddef>

namespace meios
{

// Five independent axes because no single number separates a deep auxiliary document from
// a wide one, or a long expression from a long-running one; each ceiling bounds a
// different way author-supplied input grows. A ceiling given as zero is refused and takes
// its default instead, so there is no spelling that means unbounded.
inline constexpr std::size_t default_yaml_node_ceiling = 100'000;
inline constexpr std::size_t default_yaml_depth_ceiling = 64;
inline constexpr std::size_t default_byte_ceiling = 8'000'000;
inline constexpr std::size_t default_token_ceiling = 1'000'000;
inline constexpr std::size_t default_step_ceiling = 10'000'000;

struct evaluator_limits
{
    evaluator_limits(std::size_t yaml_node_ceiling = default_yaml_node_ceiling,
                     std::size_t yaml_depth_ceiling = default_yaml_depth_ceiling,
                     std::size_t byte_ceiling = default_byte_ceiling,
                     std::size_t token_ceiling = default_token_ceiling,
                     std::size_t step_ceiling = default_step_ceiling)
        : yaml_nodes(finite(yaml_node_ceiling, default_yaml_node_ceiling)),
          yaml_depth(finite(yaml_depth_ceiling, default_yaml_depth_ceiling)),
          bytes(finite(byte_ceiling, default_byte_ceiling)),
          tokens(finite(token_ceiling, default_token_ceiling)),
          steps(finite(step_ceiling, default_step_ceiling))
    {
    }

    std::size_t yaml_nodes;
    std::size_t yaml_depth;
    std::size_t bytes;
    std::size_t tokens;
    std::size_t steps;

private:
    static std::size_t finite(std::size_t requested, std::size_t fallback)
    {
        return requested == 0 ? fallback : requested;
    }
};

struct evaluator_counters
{
    evaluator_counters() : yaml_nodes(0), yaml_depth(0), bytes(0), tokens(0), steps(0) {}

    std::size_t yaml_nodes;
    std::size_t yaml_depth;
    std::size_t bytes;
    std::size_t tokens;
    std::size_t steps;
};

}

#endif

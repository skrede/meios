#ifndef HPP_GUARD_MEIOS_XACRO_EVALUATOR_LIMITS_H
#define HPP_GUARD_MEIOS_XACRO_EVALUATOR_LIMITS_H

#include <cstddef>

namespace meios
{

// Seven independent axes because no single number separates a deep auxiliary document from a
// wide one, or a long expression from a long-running one or a deeply nested one; each
// ceiling bounds a different way author-supplied input grows. A ceiling given as zero is
// refused and takes its default instead, so there is no spelling that means unbounded.
inline constexpr std::size_t default_yaml_node_ceiling = 100'000;
inline constexpr std::size_t default_yaml_depth_ceiling = 64;
inline constexpr std::size_t default_byte_ceiling = 8'000'000;
inline constexpr std::size_t default_token_ceiling = 1'000'000;
inline constexpr std::size_t default_step_ceiling = 10'000'000;
// One recursive entry into the expression grammar is one stack frame, and a balanced
// parenthesis pair costs twelve of them. This admits twenty bracket levels, against a
// deepest measured expression of twenty-six entries and a stack exhausted at some 2 400
// entries on a 512 KiB thread.
inline constexpr std::size_t default_expression_depth_ceiling = 256;
// An alias denotes a whole anchored subtree, and nesting anchors multiplies that: measured
// with the reference loader at a fan-out of nine and one anchor per level, five levels fit in
// 258 input bytes and denote 2 790 063 expanded characters, six fit in 304 and denote
// 25 110 585. Charging denoted nodes, five levels cost 672 588 and six cost 6 053 427, so this
// admits five and refuses six. The auxiliary-node ceiling charges events, of which those two
// documents raise 45 and 54, and is therefore blind to the growth this bounds.
inline constexpr std::size_t default_alias_expansion_ceiling = 1'000'000;

struct evaluator_limits
{
    evaluator_limits(std::size_t yaml_node_ceiling = default_yaml_node_ceiling,
                     std::size_t yaml_depth_ceiling = default_yaml_depth_ceiling,
                     std::size_t byte_ceiling = default_byte_ceiling,
                     std::size_t token_ceiling = default_token_ceiling,
                     std::size_t step_ceiling = default_step_ceiling,
                     std::size_t expression_depth_ceiling = default_expression_depth_ceiling,
                     std::size_t alias_expansion_ceiling = default_alias_expansion_ceiling)
        : yaml_nodes(finite(yaml_node_ceiling, default_yaml_node_ceiling)),
          yaml_depth(finite(yaml_depth_ceiling, default_yaml_depth_ceiling)),
          bytes(finite(byte_ceiling, default_byte_ceiling)),
          tokens(finite(token_ceiling, default_token_ceiling)),
          steps(finite(step_ceiling, default_step_ceiling)),
          expression_depth(finite(expression_depth_ceiling, default_expression_depth_ceiling)),
          alias_expansion(finite(alias_expansion_ceiling, default_alias_expansion_ceiling))
    {
    }

    std::size_t yaml_nodes;
    std::size_t yaml_depth;
    std::size_t bytes;
    std::size_t tokens;
    std::size_t steps;
    std::size_t expression_depth;
    std::size_t alias_expansion;

private:
    static std::size_t finite(std::size_t requested, std::size_t fallback)
    {
        return requested == 0 ? fallback : requested;
    }
};

struct evaluator_counters
{
    evaluator_counters()
        : yaml_nodes(0), yaml_depth(0), bytes(0), tokens(0), steps(0), expression_depth(0),
          alias_expansion(0)
    {
    }

    std::size_t yaml_nodes;
    std::size_t yaml_depth;
    std::size_t bytes;
    std::size_t tokens;
    std::size_t steps;
    std::size_t expression_depth;
    std::size_t alias_expansion;
};

}

#endif

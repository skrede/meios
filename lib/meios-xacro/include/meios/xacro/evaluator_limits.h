#ifndef HPP_GUARD_MEIOS_XACRO_EVALUATOR_LIMITS_H
#define HPP_GUARD_MEIOS_XACRO_EVALUATOR_LIMITS_H

#include "meios/xacro/evaluator_constructs.h"

#include <cstddef>

namespace meios
{

// Ten independent axes because no single number separates a deep auxiliary document from a
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
// Bounds an exponentiation's base magnitude and its exponent before the work rather than its
// result after it. Defensive rather than remedial: exponentiation here is binary over checked
// fixed-width integers and refuses on the first squaring that overflows, so the spelling that
// grows without bound against arbitrary-precision integers upstream already terminates in a few
// dozen iterations here and no unbounded numeric growth exists in the current surface. Because
// it is defensive, the number is set where it refuses nothing the arithmetic beneath it accepts
// today: it is chosen rather than measured, at ten times the 99 999 999 999 that is the largest
// magnitude any recorded expression drives through exponentiation. A magnitude past it is a
// refusal where upstream would answer, which is why it sits so far above the surface.
inline constexpr std::size_t default_numeric_magnitude_ceiling = 1'000'000'000'000;
// The per-expression token count, which is the axis a tree-shaped evaluator would size by its
// tree. This evaluator builds no tree -- every precedence level computes its result immediately --
// so the one per-expression allocation is the token span index, one fixed-size row per token, and
// bounding the count bounds it directly. It is not the cumulative token ceiling in smaller
// clothing -- a session of many short expressions crosses that one without any single expression
// being wide, and one wide expression crosses this one without the session being long. The number
// is chosen rather than measured, against a longest measured expression of 29 tokens.
inline constexpr std::size_t default_expression_token_ceiling = 10'000;
// Any single string the evaluator produces, counted in bytes rather than code points. Defensive
// too: this evaluator renders no collection to text, so until concatenation and the named split
// there was no long-string producer at all, and both are linear in an expression the token
// ceilings already bound. The cumulative byte ceiling counts the same productions summed over a
// load, so a load that never builds one oversized string is still bounded in what it builds
// altogether. The number is chosen rather than measured, against a longest measured produced
// string of under a hundred bytes.
inline constexpr std::size_t default_string_length_ceiling = 1'000'000;

struct evaluator_limits
{
    evaluator_limits(std::size_t yaml_node_ceiling = default_yaml_node_ceiling,
                     std::size_t yaml_depth_ceiling = default_yaml_depth_ceiling,
                     std::size_t byte_ceiling = default_byte_ceiling,
                     std::size_t token_ceiling = default_token_ceiling,
                     std::size_t step_ceiling = default_step_ceiling,
                     std::size_t expression_depth_ceiling = default_expression_depth_ceiling,
                     std::size_t alias_expansion_ceiling = default_alias_expansion_ceiling,
                     std::size_t numeric_magnitude_ceiling = default_numeric_magnitude_ceiling,
                     std::size_t expression_token_ceiling = default_expression_token_ceiling,
                     std::size_t string_length_ceiling = default_string_length_ceiling)
        : yaml_nodes(finite(yaml_node_ceiling, default_yaml_node_ceiling)),
          yaml_depth(finite(yaml_depth_ceiling, default_yaml_depth_ceiling)),
          bytes(finite(byte_ceiling, default_byte_ceiling)),
          tokens(finite(token_ceiling, default_token_ceiling)),
          steps(finite(step_ceiling, default_step_ceiling)),
          expression_depth(finite(expression_depth_ceiling, default_expression_depth_ceiling)),
          alias_expansion(finite(alias_expansion_ceiling, default_alias_expansion_ceiling)),
          numeric_magnitude(finite(numeric_magnitude_ceiling, default_numeric_magnitude_ceiling)),
          expression_tokens(finite(expression_token_ceiling, default_expression_token_ceiling)),
          string_length(finite(string_length_ceiling, default_string_length_ceiling))
    {
    }

    std::size_t yaml_nodes;
    std::size_t yaml_depth;
    std::size_t bytes;
    std::size_t tokens;
    std::size_t steps;
    std::size_t expression_depth;
    std::size_t alias_expansion;
    std::size_t numeric_magnitude;
    std::size_t expression_tokens;
    std::size_t string_length;

private:
    static std::size_t finite(std::size_t requested, std::size_t fallback)
    {
        return requested == 0 ? fallback : requested;
    }
};

// The ten axes a load charges against the ceilings above, and beside them the set of constructs
// that load exercised. The set is not an eleventh axis: nothing is charged against it and no
// ceiling bounds it. It rides here because it is per load for the same reason the counters are,
// and because both layers that mark a construct already hold this one reference.
struct evaluator_counters
{
    evaluator_counters()
        : yaml_nodes(0), yaml_depth(0), bytes(0), tokens(0), steps(0), expression_depth(0),
          alias_expansion(0), numeric_magnitude(0), expression_tokens(0), string_length(0),
          constructs()
    {
    }

    std::size_t yaml_nodes;
    std::size_t yaml_depth;
    std::size_t bytes;
    std::size_t tokens;
    std::size_t steps;
    std::size_t expression_depth;
    std::size_t alias_expansion;
    std::size_t numeric_magnitude;
    std::size_t expression_tokens;
    std::size_t string_length;
    construct_set constructs;
};

}

#endif

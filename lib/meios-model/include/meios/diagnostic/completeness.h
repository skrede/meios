#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_COMPLETENESS_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_COMPLETENESS_H

namespace meios
{

// A caller branches on these claims through has(); nothing prints them. That is why this
// enum alone carries no string conversion: a combined value matches no case in the
// exhaustive switch its siblings use, and one handling only single flags would lie.
enum class completeness : unsigned
{
    none                = 0,
    parsed              = 1u << 0,
    topology_valid      = 1u << 1,
    deployment_complete = 1u << 2,
};

constexpr completeness operator|(completeness a, completeness b) noexcept
{
    return static_cast<completeness>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr completeness operator&(completeness a, completeness b) noexcept
{
    return static_cast<completeness>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}

inline constexpr completeness all_claims =
    completeness::parsed | completeness::topology_valid | completeness::deployment_complete;

constexpr completeness operator~(completeness claims) noexcept
{
    return static_cast<completeness>(static_cast<unsigned>(all_claims)
                                     & ~static_cast<unsigned>(claims));
}

constexpr completeness &operator|=(completeness &a, completeness b) noexcept
{
    a = a | b;
    return a;
}

constexpr completeness &operator&=(completeness &a, completeness b) noexcept
{
    a = a & b;
    return a;
}

constexpr bool has(completeness claims, completeness flag) noexcept
{
    return (claims & flag) == flag;
}

}

#endif

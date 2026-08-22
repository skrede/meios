#ifndef HPP_GUARD_MEIOS_URDF_POLICY_H
#define HPP_GUARD_MEIOS_URDF_POLICY_H

namespace meios
{

enum class material_policy
{
    fail,
    warn,
    skip,
};

enum class strictness
{
    fail,
    warn,
    skip,
};

}

#endif

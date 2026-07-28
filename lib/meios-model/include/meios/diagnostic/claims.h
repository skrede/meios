#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_CLAIMS_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_CLAIMS_H

#include "meios/diagnostic/completeness.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/captured_diagnostic.h"

#include <vector>

namespace meios
{

// The switch carries no catch-all arm on purpose: a diagnostic code added without a claim
// is then a -Wswitch error rather than a code that silently leaves every claim standing.
constexpr completeness cleared_by(diagnostic_code code) noexcept
{
    switch(code)
    {
        case diagnostic_code::unspecified:               return completeness::none;
        case diagnostic_code::cannot_open:               return completeness::parsed;
        case diagnostic_code::xml_parse_error:           return completeness::parsed;
        case diagnostic_code::non_robot_root:            return completeness::parsed;
        case diagnostic_code::invalid_topology:          return completeness::topology_valid;
        case diagnostic_code::additional_root:           return completeness::topology_valid;
        case diagnostic_code::no_root_cycle:             return completeness::topology_valid;
        case diagnostic_code::link_on_cycle:             return completeness::topology_valid;
        case diagnostic_code::undeclared_link:           return completeness::topology_valid;
        case diagnostic_code::multiple_parents:          return completeness::topology_valid;
        case diagnostic_code::unreachable_link:          return completeness::topology_valid;
        case diagnostic_code::duplicate_attribute:       return completeness::parsed;
        case diagnostic_code::additional_root_element:   return completeness::parsed;
        case diagnostic_code::trailing_content:          return completeness::parsed;
        case diagnostic_code::comment_interrupting:      return completeness::parsed;
        case diagnostic_code::undefined_material:        return completeness::deployment_complete;
        case diagnostic_code::unresolved_asset:          return completeness::deployment_complete;
        case diagnostic_code::malformed_asset_uri:       return completeness::deployment_complete;
        case diagnostic_code::lfs_pointer_asset:         return completeness::deployment_complete;
        case diagnostic_code::asset_write_failed:        return completeness::deployment_complete;
        case diagnostic_code::unsupported_uri_scheme:    return completeness::deployment_complete;
        case diagnostic_code::uncontained_asset:         return completeness::deployment_complete;
        case diagnostic_code::undefined_property:        return completeness::parsed;
        case diagnostic_code::unresolved_find:           return completeness::parsed;
        case diagnostic_code::unresolved_arg:            return completeness::parsed;
        case diagnostic_code::unresolved_env:            return completeness::parsed;
        case diagnostic_code::unknown_substitution:      return completeness::parsed;
        case diagnostic_code::unterminated_substitution: return completeness::parsed;
        case diagnostic_code::expression_error:          return completeness::parsed;
        case diagnostic_code::unsupported_expression:    return completeness::parsed;
        case diagnostic_code::unresolved_include:        return completeness::parsed;
        case diagnostic_code::xacro_parse_error:         return completeness::parsed;
        case diagnostic_code::xacro_structural_error:    return completeness::parsed;
        case diagnostic_code::expansion_budget_exceeded: return completeness::parsed;
        case diagnostic_code::vector_arity:              return completeness::parsed;
        case diagnostic_code::unknown_element:           return completeness::parsed;
        case diagnostic_code::unknown_attribute:         return completeness::parsed;
        // A simulation or control block is content the ROS wiki blesses a reader for
        // ignoring, and nearly every shipped description carries one. Clearing the parse
        // claim on them would leave it clear for every real robot, which tells a consumer
        // branching on it nothing at all.
        case diagnostic_code::extension_ignored:         return completeness::none;
        case diagnostic_code::unsupported_version:       return completeness::parsed;
        // A document whose identity or reference fields do not hold is not a document the
        // reader read: it never reached a record, so nothing downstream of it is claimed.
        case diagnostic_code::empty_name:                return completeness::parsed;
        case diagnostic_code::duplicate_name:            return completeness::parsed;
        case diagnostic_code::dangling_mimic:            return completeness::parsed;
        case diagnostic_code::no_links:                  return completeness::parsed;
        // A field the reader could not read, and an element dropped because one of its
        // required parts was missing, are both authored content that did not reach the
        // model, which is exactly what the parse claim answers for.
        case diagnostic_code::invalid_number:            return completeness::parsed;
        case diagnostic_code::missing_required_field:    return completeness::parsed;
        case diagnostic_code::missing_joint_type:        return completeness::parsed;
        case diagnostic_code::unknown_joint_type:        return completeness::parsed;
        case diagnostic_code::missing_geometry:          return completeness::parsed;
        case diagnostic_code::unknown_geometry_shape:    return completeness::parsed;
        case diagnostic_code::missing_limit:             return completeness::parsed;
        // A joint that turns about nothing, and a tensor that cannot describe a rigid body,
        // are authored content the reader would have to reinterpret to accept, so neither
        // reached the model either.
        case diagnostic_code::zero_axis:                 return completeness::parsed;
        case diagnostic_code::invalid_mass:              return completeness::parsed;
        case diagnostic_code::invalid_inertia:           return completeness::parsed;
    }
    return completeness::none;
}

inline completeness claims_from(const std::vector<captured_diagnostic> &records)
{
    completeness claims = all_claims;
    for(const captured_diagnostic &record : records)
        claims &= ~cleared_by(record.code);
    return claims;
}

}

#endif

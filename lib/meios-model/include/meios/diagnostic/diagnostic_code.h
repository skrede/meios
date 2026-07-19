#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_DIAGNOSTIC_CODE_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_DIAGNOSTIC_CODE_H

#include <string_view>

namespace meios
{

enum class diagnostic_code
{
    unspecified,
    cannot_open,
    xml_parse_error,
    non_robot_root,
    invalid_topology,
    additional_root,
    no_root_cycle,
    link_on_cycle,
    undeclared_link,
    multiple_parents,
    unreachable_link,
    duplicate_attribute,
    additional_root_element,
    trailing_content,
    comment_interrupting,
    undefined_material,
    unresolved_mesh,
    malformed_mesh_uri,
    lfs_pointer_mesh,
};

constexpr std::string_view to_string(diagnostic_code code) noexcept
{
    switch(code)
    {
        case diagnostic_code::unspecified:             return "unspecified";
        case diagnostic_code::cannot_open:             return "cannot_open";
        case diagnostic_code::xml_parse_error:         return "xml_parse_error";
        case diagnostic_code::non_robot_root:          return "non_robot_root";
        case diagnostic_code::invalid_topology:        return "invalid_topology";
        case diagnostic_code::additional_root:         return "additional_root";
        case diagnostic_code::no_root_cycle:           return "no_root_cycle";
        case diagnostic_code::link_on_cycle:           return "link_on_cycle";
        case diagnostic_code::undeclared_link:         return "undeclared_link";
        case diagnostic_code::multiple_parents:        return "multiple_parents";
        case diagnostic_code::unreachable_link:        return "unreachable_link";
        case diagnostic_code::duplicate_attribute:     return "duplicate_attribute";
        case diagnostic_code::additional_root_element: return "additional_root_element";
        case diagnostic_code::trailing_content:        return "trailing_content";
        case diagnostic_code::comment_interrupting:    return "comment_interrupting";
        case diagnostic_code::undefined_material:      return "undefined_material";
        case diagnostic_code::unresolved_mesh:         return "unresolved_mesh";
        case diagnostic_code::malformed_mesh_uri:      return "malformed_mesh_uri";
        case diagnostic_code::lfs_pointer_mesh:        return "lfs_pointer_mesh";
    }
    return "unknown";
}

}

#endif

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
    undefined_property,
    unresolved_find,
    unresolved_arg,
    unresolved_env,
    unknown_substitution,
    unterminated_substitution,
    expression_error,
    unsupported_expression,
    unresolved_include,
    xacro_parse_error,
    xacro_structural_error,
    expansion_budget_exceeded,
    vector_arity,
    unknown_element,
    unknown_attribute,
    extension_ignored,
    unsupported_version,
    empty_name,
    duplicate_name,
    dangling_mimic,
    no_links,
};

constexpr std::string_view to_string(diagnostic_code code) noexcept
{
    switch(code)
    {
        case diagnostic_code::unspecified:               return "unspecified";
        case diagnostic_code::cannot_open:               return "cannot_open";
        case diagnostic_code::xml_parse_error:           return "xml_parse_error";
        case diagnostic_code::non_robot_root:            return "non_robot_root";
        case diagnostic_code::invalid_topology:          return "invalid_topology";
        case diagnostic_code::additional_root:           return "additional_root";
        case diagnostic_code::no_root_cycle:             return "no_root_cycle";
        case diagnostic_code::link_on_cycle:             return "link_on_cycle";
        case diagnostic_code::undeclared_link:           return "undeclared_link";
        case diagnostic_code::multiple_parents:          return "multiple_parents";
        case diagnostic_code::unreachable_link:          return "unreachable_link";
        case diagnostic_code::duplicate_attribute:       return "duplicate_attribute";
        case diagnostic_code::additional_root_element:   return "additional_root_element";
        case diagnostic_code::trailing_content:          return "trailing_content";
        case diagnostic_code::comment_interrupting:      return "comment_interrupting";
        case diagnostic_code::undefined_material:        return "undefined_material";
        case diagnostic_code::unresolved_mesh:           return "unresolved_mesh";
        case diagnostic_code::malformed_mesh_uri:        return "malformed_mesh_uri";
        case diagnostic_code::lfs_pointer_mesh:          return "lfs_pointer_mesh";
        case diagnostic_code::undefined_property:        return "undefined_property";
        case diagnostic_code::unresolved_find:           return "unresolved_find";
        case diagnostic_code::unresolved_arg:            return "unresolved_arg";
        case diagnostic_code::unresolved_env:            return "unresolved_env";
        case diagnostic_code::unknown_substitution:      return "unknown_substitution";
        case diagnostic_code::unterminated_substitution: return "unterminated_substitution";
        case diagnostic_code::expression_error:          return "expression_error";
        case diagnostic_code::unsupported_expression:    return "unsupported_expression";
        case diagnostic_code::unresolved_include:        return "unresolved_include";
        case diagnostic_code::xacro_parse_error:         return "xacro_parse_error";
        case diagnostic_code::xacro_structural_error:    return "xacro_structural_error";
        case diagnostic_code::expansion_budget_exceeded: return "expansion_budget_exceeded";
        case diagnostic_code::vector_arity:              return "vector_arity";
        case diagnostic_code::unknown_element:           return "unknown_element";
        case diagnostic_code::unknown_attribute:         return "unknown_attribute";
        case diagnostic_code::extension_ignored:         return "extension_ignored";
        case diagnostic_code::unsupported_version:       return "unsupported_version";
        case diagnostic_code::empty_name:                return "empty_name";
        case diagnostic_code::duplicate_name:            return "duplicate_name";
        case diagnostic_code::dangling_mimic:            return "dangling_mimic";
        case diagnostic_code::no_links:                  return "no_links";
    }
    return "unknown";
}

}

#endif

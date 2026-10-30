#ifndef HPP_GUARD_MEIOS_TESTS_GRAMMAR_AGREEMENT_PROBES_H
#define HPP_GUARD_MEIOS_TESTS_GRAMMAR_AGREEMENT_PROBES_H

#include "agreement_probes.h"

#include <vector>

namespace differential
{

// Spelled exactly as grammar_probes.py spells them, at the same positions.
inline const std::vector<agreement_probe> grammar_agreement_probes{
        agreement_probe{"property_eager_in_unmodelled_element", "property_eager_in_unmodelled_element", {}, std::nullopt, {}, {}, {}, true},
        agreement_probe{"property_parent_scope_in_macro", "property_parent_scope_in_macro", {}},
        agreement_probe{"conditional_padded_one", "conditional_padded_one", {}},
        agreement_probe{"element_empty_name", "element_empty_name", {}, meios::diagnostic_code::xacro_structural_error, "empty name"},
        agreement_probe{"attribute_empty_name", "attribute_empty_name", {}, meios::diagnostic_code::xacro_structural_error, "empty name"},
        agreement_probe{"insert_block_undefined", "insert_block_undefined", {}, meios::diagnostic_code::xacro_structural_error, "Undefined block \"nope\""},
        agreement_probe{"env_variable_unset", "env_variable_unset", {}, meios::diagnostic_code::unresolved_env, "environment variable 'MEIOS_PROBE_UNSET' is not set"},
        agreement_probe{"call_empty_macro", "call_empty_macro", {}, meios::diagnostic_code::xacro_structural_error, "xacro:call is missing the 'macro' attribute"},
        agreement_probe{"optenv_command_without_name", "optenv_command_without_name", {}, meios::diagnostic_code::unresolved_env, "must specify an environment variable"},
        agreement_probe{"arg_command_without_name", "arg_command_without_name", {}, meios::diagnostic_code::unresolved_arg, "must specify a variable name"},
        agreement_probe{"macro_parameter_twice_read_after_call", "macro_parameter_twice_read_after_call", {}, meios::diagnostic_code::undefined_property, "name 'a' is not defined"},
        agreement_probe{"macro_parameter_twice_over_outer", "macro_parameter_twice_over_outer", {}, std::nullopt, {}, {}, {}, true},
        agreement_probe{"macro_defined_in_macro_used_after_call", "macro_defined_in_macro_used_after_call", {}, meios::diagnostic_code::xacro_structural_error, "unknown macro name: xacro:inner"},
        agreement_probe{"property_unknown_scope_in_macro", "property_unknown_scope_in_macro", {}, meios::diagnostic_code::undefined_property, "name 'p' is not defined"},
        agreement_probe{"property_evaluated_at_first_read", "property_evaluated_at_first_read", {}, std::nullopt, {}, {}, {}, true},
        agreement_probe{"property_circular_definition", "property_circular_definition", {}, meios::diagnostic_code::xacro_structural_error, "circular variable definition"},
        agreement_probe{"property_self_reference", "property_self_reference", {}, meios::diagnostic_code::xacro_structural_error, "circular variable definition"},
        agreement_probe{"property_never_read_undefined_name", "property_never_read_undefined_name", {}},
        agreement_probe{"property_never_read_dollar_run", "property_never_read_dollar_run", {}},
        agreement_probe{"property_default_reads_later_property", "property_default_reads_later_property", {}},
        agreement_probe{"property_exact_span_open_span_unread", "property_exact_span_open_span_unread", {}},
        agreement_probe{"property_reads_later_global_in_macro", "property_reads_later_global_in_macro", {}},
        agreement_probe{"property_deferred_reads_definer_table", "property_deferred_reads_definer_table", {}, meios::diagnostic_code::undefined_property, "name 'x' is not defined"},
        agreement_probe{"property_deferred_in_macro_reads_parameter", "property_deferred_in_macro_reads_parameter", {}},
        agreement_probe{"property_lazy_eval_not_boolean", "property_lazy_eval_not_boolean", {}, meios::diagnostic_code::xacro_structural_error, "Xacro conditional"},
        agreement_probe{"macro_default_forwards_deferred_global", "macro_default_forwards_deferred_global", {}},
        agreement_probe{"property_deferred_yaml_read_where_read", "property_deferred_yaml_read_where_read", {}},
        agreement_probe{"include_namespace_bare_name", "include_namespace_bare_name", {}, meios::diagnostic_code::undefined_property, "name 'w' is not defined"},
        agreement_probe{"include_namespace_dotted_property", "include_namespace_dotted_property", {}},
        agreement_probe{"namespace_property_sees_later_global", "namespace_property_sees_later_global", {}},
        agreement_probe{"namespace_falls_through_to_parent", "namespace_falls_through_to_parent", {}},
        agreement_probe{"namespace_replaced_by_second_include", "namespace_replaced_by_second_include", {}, meios::diagnostic_code::undefined_property, "name 'w' is not defined"},
        agreement_probe{"include_namespace_dotted_macro", "include_namespace_dotted_macro", {}},
        agreement_probe{"namespace_parent_scope_skips_namespace", "namespace_parent_scope_skips_namespace", {}, std::nullopt, {}, {}, {}, true},
        agreement_probe{"include_namespace_mapping_name",
                        "include_namespace_mapping_name",
                        {},
                        meios::diagnostic_code::xacro_structural_error,
                        "namespaces are supported with in-order option only"},
        agreement_probe{"namespace_macro_cannot_call_outer_macro",
                        "namespace_macro_cannot_call_outer_macro",
                        {},
                        meios::diagnostic_code::xacro_structural_error,
                        "unknown macro name: xacro:outer"},
        agreement_probe{"namespace_macro_reads_outer_global", "namespace_macro_reads_outer_global", {}},
        agreement_probe{"namespace_nested_reads", "namespace_nested_reads", {}, std::nullopt, {}, {}, {}, true},
        agreement_probe{"include_namespace_shadows_math", "include_namespace_shadows_math", {}},
        agreement_probe{"property_removed_then_read", "property_removed_then_read", {}, meios::diagnostic_code::undefined_property, "name 'b' is not defined"},
        agreement_probe{"property_value_and_remove_together",
                        "property_value_and_remove_together",
                        {},
                        meios::diagnostic_code::xacro_structural_error,
                        "Property attributes default, value, and remove are mutually exclusive: b"},
        agreement_probe{"property_removed_in_macro_reaches_global",
                        "property_removed_in_macro_reaches_global",
                        {},
                        meios::diagnostic_code::undefined_property,
                        "name 'b' is not defined"},
        agreement_probe{"property_removed_in_macro_erases_parameter",
                        "property_removed_in_macro_erases_parameter",
                        {},
                        meios::diagnostic_code::undefined_property,
                        "name 'b' is not defined"},
};

}

#endif

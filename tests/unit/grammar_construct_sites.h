#ifndef HPP_GUARD_MEIOS_UNIT_GRAMMAR_CONSTRUCT_SITES_H
#define HPP_GUARD_MEIOS_UNIT_GRAMMAR_CONSTRUCT_SITES_H

#include "grammar_constructs.h"

namespace grammar
{

inline constexpr pairing site_pairings[] = {
        {"raise.undefined_substitution_argument", carrier::agreement, "argument_command_reads_no_property argument_default_reads_itself_before_redeclaration", standing::agrees},
        {"raise.undefined_substitution_argument", carrier::generated, "span_001 span_005", standing::agrees},
        {"raise.undefined_substitution_argument", carrier::manifest, "argument_above_declaration argument_lent_from_false_conditional argument_lent_from_uncalled_macro",
         standing::open},
        {"raise.xacroexception_eval_extension", carrier::agreement, "env_variable_unset eval_reads_no_property", standing::agrees},
        {"raise.circular_variable_definition", carrier::agreement, "property_circular_definition property_self_reference", standing::agrees},
        {"raise.name", carrier::agreement, "namespace_replaced_by_second_include", standing::agrees},
        {"raise.invalid_expression_next", carrier::agreement, "dollar_run_at_end dollar_run_after_undefined_name dollar_run_before_text dollar_run_inside_expression", standing::agrees},
        {"raise.invalid_expression_next", carrier::generated, "include_005", standing::agrees},
        {"raise.namespaces_are_supported_with_in_order_option_only", carrier::agreement, "include_namespace_mapping_name", standing::agrees},
        {"raise.namespaces_are_supported_with_in_order_option_only", carrier::manifest, "include_namespace_not_text", standing::refuses_listed},
        {"raise.invalid_use_of_macro_name_call", carrier::manifest, "macro_named_call", standing::open},
        {"raise.macro_names_must_not_contain_reserved_for_namespaces", carrier::manifest, "macro_name_with_dot", standing::open},
        {"raise.property_names_must_be_valid_python_identifiers", carrier::manifest, "property_name_not_identifier", standing::open},
        {"raise.property_names_must_not_start_with_double_underscore", carrier::manifest, "property_name_double_underscore", standing::open},
        {"raise.property_attributes_default_value_and_remove_are_mutually_exclusive", carrier::agreement, "property_value_and_remove_together", standing::agrees},
        {"raise.xacroexception_handle_expr", carrier::agreement, "argument_by_bare_name dollar_run_inside_expression", standing::agrees},
        {"raise.undefined_property_to_forward", carrier::agreement, "inherited_argument_refuses", standing::agrees},
        {"raise.undefined_property_to_forward", carrier::generated, "macro_046", standing::agrees},
        {"raise.xacro_call_is_missing_the_macro_attribute", carrier::agreement, "call_empty_macro", standing::agrees},
        {"raise.unknown_macro_name", carrier::agreement, "macro_defined_in_macro_used_after_call namespace_macro_cannot_call_outer_macro", standing::agrees},
        {"raise.invalid_parameter", carrier::agreement, "macro_call_undeclared_parameter macro_call_namespace_attribute", standing::agrees},
        {"raise.invalid_parameter", carrier::generated, "macro_007", standing::agrees},
        {"raise.not_enough_blocks", carrier::generated, "macro_001", standing::agrees},
        {"raise.not_enough_blocks", carrier::generated, "macro_042 macro_048", standing::refuses_listed},
        {"raise.not_enough_blocks", carrier::manifest, "include_sole_block", standing::open},
        {"raise.unused_block", carrier::agreement, "macro_call_unused_block macro_call_unused_blocks", standing::agrees},
        {"raise.unused_block", carrier::generated, "macro_005", standing::agrees},
        {"raise.undefined_parameters", carrier::agreement, "macro_default_empty", standing::agrees},
        {"raise.xacro_conditional", carrier::agreement, "property_lazy_eval_not_boolean property_remove_not_boolean", standing::agrees},
        {"raise.xacro_conditional", carrier::manifest, "conditional_real_one conditional_upper_true conditional_empty", standing::open},
        {"raise.xacro_conditional", carrier::manifest, "conditional_quoted_text", standing::refuses_listed},
        {"raise.undefined_block", carrier::agreement, "insert_block_undefined", standing::agrees},
        {"raise.xacro_element_empty_name", carrier::agreement, "element_empty_name", standing::agrees},
        {"raise.xacro_attribute_empty_name", carrier::agreement, "attribute_empty_name", standing::agrees},
        {"raise.xacroexception_parse", carrier::agreement, "include_find_composed include_escaped_find", standing::agrees},
        {"raise.environment_variable", carrier::agreement, "env_variable_unset", standing::agrees},
        {"raise.env_var_command_only_accepts_one_argument", carrier::manifest, "env_command_two_words", standing::open},
        {"raise.optenv_var_must_specify_an_environment_variable", carrier::agreement, "optenv_command_without_name", standing::agrees},
        {"raise.cannot_substitute_dirname_no_file_directory_information_available", carrier::manifest, "dirname_command", standing::open},
        {"raise.find_pkg_accepts_exactly_one_argument", carrier::agreement, "find_command_second_word include_find_two_words", standing::agrees},
        {"raise.find_pkg_accepts_exactly_one_argument", carrier::generated, "include_006", standing::agrees},
        {"raise.argexception_eval_arg", carrier::agreement, "argument_command_reads_no_property", standing::agrees},
        {"raise.arg_var_must_specify_a_variable_name", carrier::agreement, "arg_command_without_name", standing::agrees},
        {"raise.arg_var_may_only_specify_one_arg", carrier::agreement, "argument_command_second_word", standing::agrees},
        {"raise.arg_var_may_only_specify_one_arg", carrier::generated, "span_002", standing::agrees},
        {"raise.eval_may_not_contain_double_underscore_expressions", carrier::agreement, "eval_double_underscore", standing::agrees},
        {"raise.unknown_substitution_command", carrier::agreement, "arg_command_tab_before_name find_command_tab_before_package", standing::agrees},
        {"raise.unknown_substitution_command", carrier::manifest, "eval_command_padded", standing::open},
        {"raise.unknown_substitution_command", carrier::manifest, "eval_command_empty", standing::refuses_listed},
        {"raise.dollar_signs_cannot_be_inside_of_substitution_args", carrier::agreement, "command_dollar_in_body", standing::agrees},
        {"raise.invalid_left_parenthesis_in_substitution_args", carrier::agreement, "command_expanded_parenthesis command_parenthesis_in_body", standing::agrees},
        {"raise.missing_attribute", carrier::manifest, "property_without_name macro_without_name conditional_without_value unless_without_value", standing::open},
        {"raise.missing_attribute", carrier::manifest, "include_without_filename", standing::refuses_listed},
        {"raise.substitution_args_not_supported", carrier::manifest, "find_without_package_index", standing::refuses_listed},

        {"warning.unknown_attribute_s", carrier::unit, "xacro_property.an attribute outside a property's attribute list raises no warning here", standing::agrees},
        {"warning.redefining_global_symbol", carrier::agreement, "property_shadows_pi", standing::agrees},
        {"warning.redefining_global_symbol", carrier::unit, "xacro_property.a property named pi shadows the built-in and warns that it redefines a global symbol", standing::agrees},
        {"warning.cannot_remove_global_symbol", carrier::agreement, "property_removes_builtin", standing::agrees},
        {"warning.cannot_remove_global_symbol", carrier::unit, "xacro_property.removing a built-in keeps it and warns that a global symbol cannot be removed", standing::agrees},
        {"warning.include_tag_s_filename_spec", carrier::manifest, "include_glob_matching_nothing", standing::refuses_listed},
        {"warning.inconsistent_namespace_redefinitions_for", carrier::unit, "xacro_include_read.an include redeclaring a namespace prefix raises no warning here", standing::agrees},
        {"warning.child_elements_of_a_xacro_include_tag_are_ignored", carrier::unit, "xacro_include_read.an include's child elements are ignored with no warning here",
         standing::agrees},
        {"warning.macro_names_must_not_contain_prefix_xacro", carrier::manifest, "macro_name_with_xacro_prefix", standing::refuses_listed},
        {"warning.default_property_value_can_only_be_defined_on_local_scope", carrier::unit, "xacro_property.a default beside a scope binds and raises no warning here",
         standing::agrees},

        {"raise.the_yaml_dictionary_has_no_key", carrier::boundary, "expression", standing::open},
        {"raise.invalid_expression", carrier::boundary, "expression", standing::open},
        {"raise.yaml_support_not_available_install_python_yaml", carrier::boundary, "expression", standing::open},
        {"warning.wrapper", carrier::boundary, "expression", standing::open},
        {"raise.xacroexception_fatal", carrier::boundary, "expression", standing::open},
        {"raise.use_of_invalid_name_s", carrier::boundary, "expression", standing::open},
        {"raise.failed_to_open_output", carrier::boundary, "tool", standing::open},
};

inline constexpr unreached_site unreached_sites[] = {
        {"raise.variable_filename_is_supported_with_in_order_option_only", "reached only without a symbol table, and in-order processing always passes one"},
        {"warning.no_parent_scope_at_global_scope", "a table's parent is never None: Table substitutes an empty dict"},
        {"raise.is_not_a", "convert_value reaches the bool branch only after a true or false check"},
        {"raise.valueerror_convert_value", "convert_value is called with auto or bool alone, never yaml"},
        {"raise.unknown_type", "convert_value is called with auto or bool alone"},
};

}

#endif

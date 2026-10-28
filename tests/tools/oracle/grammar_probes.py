# The grammar record's document probes: constructs of xacro's own grammar measured against the
# pinned reference, each recorded as a divergence until the two sides agree.

import os

GRAMMAR_DOCUMENT_PROBES = ("property_evaluated_at_first_read",
                           "property_circular_definition",
                           "property_self_reference",
                           "property_never_read_undefined_name",
                           "property_never_read_dollar_run",
                           "property_default_reads_later_property",
                           "property_removed_then_read",
                           "property_value_and_remove_together",
                           "include_namespace_bare_name",
                           "include_namespace_dotted_property",
                           "include_namespace_dotted_macro",
                           "include_optional_missing_file",
                           "macro_called_by_name_attribute",
                           "element_named_by_attribute",
                           "attribute_set_on_parent",
                           "property_block_body_inserted",
                           "property_name_from_expression",
                           "property_name_not_identifier",
                           "property_name_double_underscore",
                           "root_target_namespace",
                           "include_find_answering_root_only",
                           "macro_params_unicode_space_between",
                           "argument_lent_from_false_conditional",
                           "argument_lent_from_uncalled_macro",
                           "macro_parameter_twice_read_after_call",
                           "macro_parameter_twice_over_outer",
                           "property_exact_span_open_span_unread",
                           "include_missing_element_before_parent",
                           "include_missing_element_before_parent_nested",
                           "include_find_missing_element_before_parent",
                           "include_held_find_in_property",
                           "include_held_find_in_argument",
                           "include_held_find_directory",
                           "include_find_composed_reads_neighbor",
                           "include_find_trailing_separator",
                           "cwd_command",
                           "eval_command_padded",
                           "eval_command_empty",
                           "eval_command_blank",
                           "env_command_two_words",
                           "optenv_command_fallback_words",
                           "empty_command",
                           "yaml_empty_scalar_key",
                           "property_quoted_value",
                           "property_padded_number",
                           "property_signed_number",
                           "property_unicode_digit",
                           "property_infinity_spelling",
                           "macro_argument_literal_reading",
                           "property_shadows_pi",
                           "conditional_real_one",
                           "conditional_upper_true",
                           "conditional_empty",
                           "conditional_quoted_text",
                           "property_without_name",
                           "macro_without_name",
                           "conditional_without_value",
                           "unless_without_value",
                           "include_without_filename",
                           "macro_named_call",
                           "macro_name_with_dot",
                           "macro_name_with_xacro_prefix",
                           "macro_defined_in_macro_used_after_call",
                           "property_removed_in_macro_reaches_global",
                           "property_removed_in_macro_erases_parameter")

# Each root is joined under the directory the probe is staged in, in the listed order, and @STAGE@
# in an argument value is that directory resolved.
GRAMMAR_PROBE_ARGUMENTS = {"cwd_command": {"here": "@STAGE@"}}
GRAMMAR_PROBE_ROOTS = {"include_find_answering_root_only": ("include_find_answering_root_only.d/r1",
                                                            "include_find_answering_root_only.d/r2"),
                       **{one: (one + ".d",) for one in ("include_find_missing_element_before_parent",
                                                         "include_held_find_in_property",
                                                         "include_held_find_in_argument",
                                                         "include_held_find_directory",
                                                         "include_find_composed_reads_neighbor",
                                                         "include_find_trailing_separator")}}
GRAMMAR_PROBES_IN_STAGE = ("cwd_command",)


# Both sides read this one fixed environment, so no row spells a machine's own.
def probe_environment():
    os.environ["MEIOS_PROBE_ENV"] = "probe_value"
    os.environ.pop("MEIOS_PROBE_UNSET", None)

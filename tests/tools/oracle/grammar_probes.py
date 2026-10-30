# The grammar record's document probes: constructs of xacro's own grammar measured against the
# pinned reference, each recorded as a divergence until the two sides agree.

import os
import sys
import contextlib
from pathlib import Path

HERE = Path(__file__).resolve().parent

GRAMMAR_DOCUMENT_PROBES = ("include_optional_missing_file",
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
                           "comment_kept_in_output",
                           "block_evaluated_where_inserted",
                           "block_parameter_seen_by_nested_call",
                           "block_property_before_block_parameter",
                           "xacro_attribute_removed",
                           "xacro_attribute_never_evaluated",
                           "included_root_namespace_lifted",
                           "macro_namespace_lifted",
                           "cdata_kept_verbatim",
                           "attribute_under_conditional",
                           "attribute_under_macro_body",
                           "attribute_under_included_root",
                           "attribute_under_block_wrapper",
                           "attribute_under_block_element",
                           "attribute_on_document_root",
                           "element_renamed_to_conditional",
                           "element_renamed_to_macro_call",
                           "comment_evaluated_under_eval_comments",
                           "comment_before_xacro_element_removed",
                           "processing_instruction_kept",
                           "include_glob_matching_nothing",
                           "include_namespace_not_text",
                           "comment_evaluation_switched_off",
                           "find_without_package_index",
                           "property_chain_deeper_than_depth_ceiling",
                           "include_namespace_read_as_value")

# The grammar record's agreement probes, shaped as AGREEMENT_PROBES and rendered with it: constructs
# both sides already render or refuse alike, each held to a fresh render so losing the agreement fails.
GRAMMAR_AGREEMENT_PROBES = (("property_eager_in_unmodelled_element", "property_eager_in_unmodelled_element", None),
                            ("property_parent_scope_in_macro", "property_parent_scope_in_macro", None),
                            ("conditional_padded_one", "conditional_padded_one", None),
                            ("element_empty_name", "element_empty_name", None),
                            ("attribute_empty_name", "attribute_empty_name", None),
                            ("insert_block_undefined", "insert_block_undefined", None),
                            ("env_variable_unset", "env_variable_unset", None),
                            ("call_empty_macro", "call_empty_macro", None),
                            ("optenv_command_without_name", "optenv_command_without_name", None),
                            ("arg_command_without_name", "arg_command_without_name", None),
                            ("macro_parameter_twice_read_after_call", "macro_parameter_twice_read_after_call", None),
                            ("macro_parameter_twice_over_outer", "macro_parameter_twice_over_outer", None),
                            ("macro_defined_in_macro_used_after_call", "macro_defined_in_macro_used_after_call", None),
                            ("property_unknown_scope_in_macro", "property_unknown_scope_in_macro", None),
                            ("property_evaluated_at_first_read", "property_evaluated_at_first_read", None),
                            ("property_circular_definition", "property_circular_definition", None),
                            ("property_self_reference", "property_self_reference", None),
                            ("property_never_read_undefined_name", "property_never_read_undefined_name", None),
                            ("property_never_read_dollar_run", "property_never_read_dollar_run", None),
                            ("property_default_reads_later_property", "property_default_reads_later_property", None),
                            ("property_exact_span_open_span_unread", "property_exact_span_open_span_unread", None),
                            ("property_reads_later_global_in_macro", "property_reads_later_global_in_macro", None),
                            ("property_deferred_reads_definer_table", "property_deferred_reads_definer_table", None),
                            ("property_deferred_in_macro_reads_parameter", "property_deferred_in_macro_reads_parameter", None),
                            ("property_lazy_eval_not_boolean", "property_lazy_eval_not_boolean", None),
                            ("macro_default_forwards_deferred_global", "macro_default_forwards_deferred_global", None),
                            ("property_deferred_yaml_read_where_read", "property_deferred_yaml_read_where_read", None),
                            ("include_namespace_bare_name", "include_namespace_bare_name", None),
                            ("include_namespace_dotted_property", "include_namespace_dotted_property", None),
                            ("namespace_property_sees_later_global", "namespace_property_sees_later_global", None),
                            ("namespace_falls_through_to_parent", "namespace_falls_through_to_parent", None),
                            ("namespace_replaced_by_second_include", "namespace_replaced_by_second_include", None),
                            ("include_namespace_dotted_macro", "include_namespace_dotted_macro", None),
                            ("namespace_parent_scope_skips_namespace", "namespace_parent_scope_skips_namespace", None),
                            ("include_namespace_mapping_name", "include_namespace_mapping_name", None),
                            ("namespace_macro_cannot_call_outer_macro", "namespace_macro_cannot_call_outer_macro", None),
                            ("namespace_macro_reads_outer_global", "namespace_macro_reads_outer_global", None),
                            ("namespace_nested_reads", "namespace_nested_reads", None),
                            ("include_namespace_shadows_math", "include_namespace_shadows_math", None),
                            ("property_removed_then_read", "property_removed_then_read", None),
                            ("property_value_and_remove_together", "property_value_and_remove_together", None),
                            ("property_removed_in_macro_reaches_global", "property_removed_in_macro_reaches_global", None),
                            ("property_removed_in_macro_erases_parameter", "property_removed_in_macro_erases_parameter", None),
                            ("property_shadows_pi", "property_shadows_pi", None),
                            ("parent_scope_at_document_level", "parent_scope_at_document_level", None),
                            ("property_removes_builtin", "property_removes_builtin", None))

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
# Rendered by the pinned install as it ships, which has no package index: the vendored one is what
# lets every other probe's find resolve.
GRAMMAR_PROBES_WITHOUT_PACKAGE_INDEX = ("find_without_package_index",)


# Both sides read this one fixed environment, so no row spells a machine's own.
def probe_environment():
    os.environ["MEIOS_PROBE_ENV"] = "probe_value"
    os.environ.pop("MEIOS_PROBE_UNSET", None)


# The vendored index is taken off the import path and out of the module cache while the probe
# renders, so upstream's own import of it fails as it does in the bare pinned install.
@contextlib.contextmanager
def without_package_index(case_id):
    if case_id not in GRAMMAR_PROBES_WITHOUT_PACKAGE_INDEX:
        yield
        return
    saved_path = list(sys.path)
    saved_modules = {name: sys.modules.pop(name) for name in list(sys.modules)
                     if name.split(".")[0] == "ament_index_python"}
    try:
        sys.path[:] = [one for one in sys.path if Path(one or ".").resolve() != HERE]
        yield
    finally:
        sys.path[:] = saved_path
        sys.modules.update(saved_modules)

# The probe tables differential.py renders. Spelled exactly as the C++ comparator's own tables
# spell them, so an id names the same fixture and the same rendered file whichever side wrote it.

import os

from recorder.corpus import REPO

# The known divergences, measured and deliberately not changed: and/or yielding a boolean rather
# than the deciding operand, a self-referential alias graph that loads here as a value containing
# itself, the two mapping-constructor argument shapes upstream accepts that are refused here, the
# four string meanings upstream carries that this grammar does not -- repetition, ordering, and
# the split's two further argument shapes -- the one mathematics name this grammar answers and
# upstream does not, and three differences in what the span scanner hands the evaluator,
# upstream's own span pattern being ^\$\{[^\}]*\} and so neither quote-aware nor nesting. None
# needs the expressions.cases seed scope -- the third reads only the document written beside the
# probe, which load_yaml resolves relative to the probe itself.
DIVERGENCE_PROBES = (("div_or_operand", "1 or 2"),
                     ("div_and_operand", "2 and 3"),
                     ("div_self_reference", "xacro.load_yaml('recursive.yaml')['a']"),
                     ("div_dict_pair_sequence", "dict([('a', 1)])"),
                     ("div_dict_mixed_arguments", "dict([('a', 1)], b=2)"),
                     ("div_string_repetition", "'ab' * 3"),
                     ("div_string_ordering", "'a' < 'b'"),
                     ("div_string_split_whitespace", "'a b'.split()"),
                     ("div_string_split_limit", "'a b c'.split(' ', 1)"),
                     ("div_absolute_value", "abs(-3)"),
                     ("div_span_closing_brace_in_literal", "'a}b'"),
                     ("div_span_format_operator", "'%.3f' % 1.2345"),
                     ("div_span_nested_expression", "${'1 + 2'}"))
RECURSIVE_YAML = "a: &a [1, *a]\n"

# A difference needing a macro, a conditional and a call site has no expression to wrap, so its
# probe is a whole committed document read from tests/fixtures/xacro/probes by both sides. Ids
# only: the document is the fixture, and repeating it here is what a rename would silently defeat.
DOCUMENT_PROBES = ("non_text_mapping_key",
                   "argument_without_default",
                   "argument_helper_call",
                   "argument_above_declaration",
                   "include_parent_directory",
                   "include_absolute_path",
                   "include_sole_block",
                   "dirname_command",
                   "include_outside_link",
                   "include_package_filename",
                   "find_relative_root",
                   "find_dotted_root")
PROBE_DIR = REPO / "tests" / "fixtures" / "xacro" / "probes"

# A committed fixture cannot spell a machine's absolute path, so these probes are handed the
# directory they are staged in under this argument name.
PROBE_DIRECTORY_ARGUMENT = "probe_directory"
DIRECTORY_BOUND_PROBES = ("include_absolute_path",)

# These probes are handed their package root as a caller hands one, from the directory they are
# staged in: the relative root as written, and the dotted root joined under that directory, which a
# committed fixture cannot spell. A relative root resolves against that working directory.
RELATIVE_ROOT_PROBES = {"find_relative_root": "find_relative_root.d"}
DOTTED_ROOT_PROBES = {"find_dotted_root": "./find_dotted_root.d"}


def probe_root(case_id, directory):
    if case_id in DOTTED_ROOT_PROBES:
        return os.path.join(str(directory), DOTTED_ROOT_PROBES[case_id])
    return RELATIVE_ROOT_PROBES.get(case_id)

# Minimized binding documents both sides must render alike, compared whole: verdict id, fixture,
# and the caller mapping where the binding needs one. A probe both sides refuse agrees only on the
# cause, which the comparator's own table names for each side.
AGREEMENT_PROBES = (("property_default_over_argument", "property_default_over_argument", None),
                    ("property_default_over_caller_argument", "property_default_over_argument", {"p": "C"}),
                    ("property_value_over_argument", "property_value_over_argument", None),
                    ("property_before_argument", "property_before_argument", None),
                    ("global_property_over_argument", "global_property_over_argument", None),
                    ("argument_beside_parameter", "argument_beside_parameter", None),
                    ("property_default_under_parameter", "property_default_under_parameter", None),
                    ("property_default_reads_argument", "property_default_reads_argument", None),
                    ("property_default_reads_caller_argument", "property_default_reads_argument",
                     {"cfg": "from_caller"}),
                    ("argument_by_bare_name", "argument_by_bare_name", None),
                    ("argument_command_reads_no_property", "argument_command_reads_no_property", None),
                    ("inherited_default_skips_argument", "inherited_default_skips_argument", None),
                    ("inherited_argument_refuses", "inherited_argument_refuses", None),
                    ("eval_reads_argument_over_property", "eval_reads_argument_over_property", None),
                    ("eval_string_argument", "eval_string_argument", None),
                    ("eval_string_caller_argument", "eval_string_argument", {"p": "C"}),
                    ("eval_string_condition", "eval_string_condition", None),
                    ("eval_argument_typing", "eval_argument_typing", None),
                    ("eval_shadowed_names", "eval_shadowed_names", None),
                    ("eval_reads_no_property", "eval_reads_no_property", None),
                    ("eval_nested_command", "eval_nested_command", None),
                    ("eval_parenthesis_ends_command", "eval_parenthesis_ends_command", None),
                    ("command_nested_in_default", "command_nested_in_default", None),
                    ("command_nested_beside_bound_argument", "command_nested_beside_bound_argument", None),
                    ("command_nested_in_find", "command_nested_in_find", None),
                    ("command_parenthesis_in_body", "command_parenthesis_in_body", None),
                    ("command_dollar_in_body", "command_dollar_in_body", None),
                    ("command_expanded_argument_name", "command_expanded_argument_name", None),
                    ("command_expanded_parenthesis", "command_expanded_parenthesis", None),
                    ("block_property_never_inserted", "block_property_never_inserted", None),
                    ("block_global_call_never_inserted", "block_global_call_never_inserted", None),
                    ("block_emitting_call_never_inserted", "block_emitting_call_never_inserted", None),
                    ("block_unbound_never_inserted", "block_unbound_never_inserted", None),
                    ("block_unbound_discarded", "discarded_block", None),
                    ("eval_unicode_digits", "eval_unicode_digits", None),
                    ("eval_double_underscore", "eval_double_underscore", None),
                    ("argument_command_second_word", "argument_command_second_word", None),
                    ("argument_command_second_word_bound", "argument_command_second_word_bound", None),
                    ("argument_command_expression_name", "argument_command_expression_name", None),
                    ("argument_command_expression_words", "argument_command_expression_words", None),
                    ("include_beside_document", "include_beside_document", None),
                    ("include_nested_beside", "include_nested_beside", None),
                    ("include_beside_over_root", "include_nested_beside", None),
                    ("include_in_macro_body", "include_in_macro_body", None),
                    ("block_include_never_inserted", "block_include_never_inserted", None),
                    ("include_linked_document", "include_linked_document", None),
                    ("include_parent_directory_climbed", "include_parent_directory", None),
                    ("include_outside_link_followed", "include_outside_link", None),
                    ("macro_call_undeclared_parameter", "macro_call_undeclared_parameter", None),
                    ("macro_call_namespace_attribute", "macro_call_namespace_attribute", None),
                    ("macro_call_block_named_attribute", "macro_call_block_named_attribute", None),
                    ("macro_call_unused_block", "macro_call_unused_block", None),
                    ("macro_call_unused_blocks", "macro_call_unused_blocks", None),
                    ("macro_call_undeclared_before_missing", "macro_call_undeclared_before_missing", None),
                    ("macro_call_undeclared_before_block", "macro_call_undeclared_before_block", None),
                    ("macro_call_attribute_before_child_property", "macro_call_attribute_before_child_property", None),
                    ("macro_call_unused_block_before_missing", "macro_call_unused_block_before_missing", None),
                    ("macro_call_undeclared_beside_absent_block", "macro_call_undeclared_beside_absent_block", None),
                    ("macro_call_undeclared_before_bad_attribute", "macro_call_undeclared_before_bad_attribute", None),
                    ("macro_default_empty", "macro_default_empty", None),
                    ("macro_call_default_reads_child_property", "macro_call_default_reads_child_property", None),
                    ("macro_call_property_child_no_block", "macro_call_property_child_no_block", None),
                    ("macro_default_spans_whitespace", "macro_default_spans_whitespace", None),
                    ("macro_default_argument_command", "macro_default_argument_command", None),
                    ("macro_param_spaced_separator", "macro_param_spaced_separator", None),
                    ("dollar_run_escapes_openers", "dollar_run_escapes_openers", None),
                    ("dollar_run_before_text", "dollar_run_before_text", None),
                    ("dollar_run_at_end", "dollar_run_at_end", None),
                    ("dollar_run_inside_expression", "dollar_run_inside_expression", None),
                    ("include_escaped_find", "include_escaped_find", None),
                    ("dollar_run_in_argument_default", "dollar_run_in_argument_default", None),
                    ("dollar_run_in_unused_argument_default", "dollar_run_in_unused_argument_default", None),
                    ("dollar_run_in_redeclared_argument_default", "dollar_run_in_redeclared_argument_default", None),
                    ("argument_redeclared_after_refusing_default", "argument_redeclared_after_refusing_default", None),
                    ("argument_redeclared_after_expression_default", "argument_redeclared_after_expression_default", None),
                    ("argument_declared_in_skipped_branch", "argument_declared_in_skipped_branch", None),
                    ("argument_declared_in_uncalled_macro", "argument_declared_in_uncalled_macro", None),
                    ("argument_declared_in_include_first", "argument_declared_in_include_first", None),
                    ("dollar_run_after_undefined_name", "dollar_run_after_undefined_name", None),
                    ("dollar_run_after_unset_argument", "dollar_run_after_unset_argument", None),
                    ("dollar_run_after_division_by_zero", "dollar_run_after_division_by_zero", None),
                    ("unterminated_span_after_undefined_name", "unterminated_span_after_undefined_name", None))

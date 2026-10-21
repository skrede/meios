#ifndef HPP_GUARD_MEIOS_TESTS_AGREEMENT_PROBES_H
#define HPP_GUARD_MEIOS_TESTS_AGREEMENT_PROBES_H

#include <meios/xacro.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <map>
#include <string>
#include <vector>
#include <optional>
#include <string_view>

namespace differential
{

// A minimized binding document both sides must render alike, under the caller arguments the
// binding needs. The first three fields are spelled exactly as differential.py's own tuple spells
// them, so an id names the same fixture and the same rendered file whichever side wrote it. A
// probe both sides refuse also names the cause each side must give. The root, relative to the
// probe directory, is a configured source this side alone is handed: upstream consults none. The
// include options, like the root, are this side's alone, because upstream always reads what they
// admit.
struct agreement_probe
{
    std::string_view id;
    std::string_view fixture;
    std::map<std::string, std::string> args;
    std::optional<meios::diagnostic_code> refuses;
    std::string_view upstream_cause;
    std::string_view root;
    meios::include_options includes;
};

inline meios::include_options follows_outside_links()
{
    meios::include_options options;
    options.follow_outside_links = true;
    return options;
}

inline meios::include_options climbs_above_document()
{
    meios::include_options options;
    options.climb_above_document = true;
    return options;
}

inline const std::vector<agreement_probe> agreement_probes{
        agreement_probe{"property_default_over_argument", "property_default_over_argument", {}},
        agreement_probe{"property_default_over_caller_argument", "property_default_over_argument", {{"p", "C"}}},
        agreement_probe{"property_value_over_argument", "property_value_over_argument", {}},
        agreement_probe{"property_before_argument", "property_before_argument", {}},
        agreement_probe{"global_property_over_argument", "global_property_over_argument", {}},
        agreement_probe{"argument_beside_parameter", "argument_beside_parameter", {}},
        agreement_probe{"property_default_under_parameter", "property_default_under_parameter", {}},
        agreement_probe{"property_default_reads_argument", "property_default_reads_argument", {}},
        agreement_probe{"property_default_reads_caller_argument", "property_default_reads_argument", {{"cfg", "from_caller"}}},
        agreement_probe{"argument_by_bare_name", "argument_by_bare_name", {}, meios::diagnostic_code::undefined_property, "name 'p' is not defined"},
        agreement_probe{"argument_command_reads_no_property", "argument_command_reads_no_property", {}, meios::diagnostic_code::unresolved_arg, "Undefined substitution argument p"},
        agreement_probe{"inherited_default_skips_argument", "inherited_default_skips_argument", {}},
        agreement_probe{"inherited_argument_refuses", "inherited_argument_refuses", {}, meios::diagnostic_code::xacro_structural_error, "Undefined property to forward: p"},
        agreement_probe{"eval_reads_argument_over_property", "eval_reads_argument_over_property", {}},
        agreement_probe{"eval_string_argument", "eval_string_argument", {}},
        agreement_probe{"eval_string_caller_argument", "eval_string_argument", {{"p", "C"}}},
        agreement_probe{"eval_string_condition", "eval_string_condition", {}},
        agreement_probe{"eval_argument_typing", "eval_argument_typing", {}},
        agreement_probe{"eval_shadowed_names", "eval_shadowed_names", {}},
        agreement_probe{"eval_reads_no_property", "eval_reads_no_property", {}, meios::diagnostic_code::undefined_property, "name 'w' is not defined"},
        agreement_probe{"eval_nested_command", "eval_nested_command", {}, meios::diagnostic_code::unterminated_substitution, "invalid expression: $(arg q"},
        agreement_probe{"eval_parenthesis_ends_command", "eval_parenthesis_ends_command", {}, meios::diagnostic_code::expression_error, "SyntaxError"},
        agreement_probe{"command_nested_in_default", "command_nested_in_default", {}, meios::diagnostic_code::unterminated_substitution, "invalid expression: $(arg q"},
        agreement_probe{
                "command_nested_beside_bound_argument", "command_nested_beside_bound_argument", {}, meios::diagnostic_code::unterminated_substitution, "invalid expression: $(arg q"},
        agreement_probe{"command_nested_in_find", "command_nested_in_find", {}, meios::diagnostic_code::unterminated_substitution, "invalid expression: $(arg pkg"},
        agreement_probe{"command_parenthesis_in_body", "command_parenthesis_in_body", {}, meios::diagnostic_code::unterminated_substitution, "Invalid left parenthesis"},
        agreement_probe{"command_dollar_in_body", "command_dollar_in_body", {}, meios::diagnostic_code::unterminated_substitution, "Dollar signs"},
        agreement_probe{"command_expanded_argument_name", "command_expanded_argument_name", {}},
        agreement_probe{"command_expanded_parenthesis", "command_expanded_parenthesis", {}, meios::diagnostic_code::unterminated_substitution, "Invalid left parenthesis"},
        agreement_probe{"block_property_never_inserted", "block_property_never_inserted", {}},
        agreement_probe{"block_global_call_never_inserted", "block_global_call_never_inserted", {}},
        agreement_probe{"block_emitting_call_never_inserted", "block_emitting_call_never_inserted", {}},
        agreement_probe{
                "block_unbound_never_inserted", "block_unbound_never_inserted", {}, meios::diagnostic_code::undefined_property, "name 'undefined_name_nobody_bound' is not defined"},
        agreement_probe{"block_unbound_discarded", "discarded_block", {}, meios::diagnostic_code::undefined_property, "name 'undefined_name_nobody_bound' is not defined"},
        agreement_probe{"eval_unicode_digits", "eval_unicode_digits", {}},
        agreement_probe{"eval_double_underscore", "eval_double_underscore", {}, meios::diagnostic_code::expression_error, "double underscore"},
        agreement_probe{"argument_command_second_word", "argument_command_second_word", {}, meios::diagnostic_code::unresolved_arg, "may only specify one arg"},
        agreement_probe{"argument_command_second_word_bound", "argument_command_second_word_bound", {}, meios::diagnostic_code::unresolved_arg, "may only specify one arg"},
        agreement_probe{"argument_command_expression_name", "argument_command_expression_name", {}},
        agreement_probe{"argument_command_expression_words", "argument_command_expression_words", {}, meios::diagnostic_code::unresolved_arg, "may only specify one arg"},
        agreement_probe{"include_beside_document", "include_beside_document", {}},
        agreement_probe{"include_nested_beside", "include_nested_beside", {}},
        agreement_probe{"include_beside_over_root", "include_nested_beside", {}, std::nullopt, {}, "."},
        agreement_probe{"include_in_macro_body", "include_in_macro_body", {}},
        agreement_probe{"block_include_never_inserted", "block_include_never_inserted", {}},
        agreement_probe{"include_linked_document", "include_linked_document", {}},
        agreement_probe{"include_parent_directory_climbed", "include_parent_directory", {}, std::nullopt, {}, {}, climbs_above_document()},
        agreement_probe{"include_outside_link_followed", "include_outside_link", {}, std::nullopt, {}, {}, follows_outside_links()},
        agreement_probe{"macro_call_undeclared_parameter", "macro_call_undeclared_parameter", {}, meios::diagnostic_code::xacro_structural_error, "Invalid parameter \"b\""},
        agreement_probe{"macro_call_namespace_attribute", "macro_call_namespace_attribute", {}, meios::diagnostic_code::xacro_structural_error, "Invalid parameter \"xmlns:foo\""},
        agreement_probe{"macro_call_block_named_attribute", "macro_call_block_named_attribute", {}, meios::diagnostic_code::xacro_structural_error, "Invalid parameter \"blk\""},
        agreement_probe{"macro_call_unused_block", "macro_call_unused_block", {}, meios::diagnostic_code::xacro_structural_error, "Unused block \"visual\""},
        agreement_probe{"macro_call_unused_blocks", "macro_call_unused_blocks", {}, meios::diagnostic_code::xacro_structural_error, "Unused block \"first\""},
        agreement_probe{"macro_call_undeclared_before_missing", "macro_call_undeclared_before_missing", {}, meios::diagnostic_code::xacro_structural_error, "Invalid parameter \"b\""},
        agreement_probe{"macro_call_undeclared_before_block", "macro_call_undeclared_before_block", {}, meios::diagnostic_code::xacro_structural_error, "Invalid parameter \"b\""},
        agreement_probe{
                "macro_call_attribute_before_child_property", "macro_call_attribute_before_child_property", {}, meios::diagnostic_code::undefined_property, "name 'p' is not defined"},
        agreement_probe{
                "macro_call_unused_block_before_missing", "macro_call_unused_block_before_missing", {}, meios::diagnostic_code::xacro_structural_error, "Unused block \"visual\""},
        agreement_probe{
                "macro_call_undeclared_beside_absent_block", "macro_call_undeclared_beside_absent_block", {}, meios::diagnostic_code::xacro_structural_error, "Invalid parameter \"b\""},
        agreement_probe{"macro_call_undeclared_before_bad_attribute",
                        "macro_call_undeclared_before_bad_attribute",
                        {},
                        meios::diagnostic_code::xacro_structural_error,
                        "Invalid parameter \"b\""},
        agreement_probe{"macro_default_empty", "macro_default_empty", {}, meios::diagnostic_code::xacro_structural_error, "Undefined parameters [a]"},
        agreement_probe{"macro_call_default_reads_child_property", "macro_call_default_reads_child_property", {}},
        agreement_probe{"macro_call_property_child_no_block", "macro_call_property_child_no_block", {}},
        agreement_probe{"macro_default_spans_whitespace", "macro_default_spans_whitespace", {}},
        agreement_probe{"macro_default_argument_command", "macro_default_argument_command", {}},
        agreement_probe{"macro_param_spaced_separator", "macro_param_spaced_separator", {}},
        agreement_probe{"dollar_run_escapes_openers", "dollar_run_escapes_openers", {}},
        agreement_probe{"dollar_run_before_text", "dollar_run_before_text", {}, meios::diagnostic_code::expression_error, "invalid expression: $$x"},
        agreement_probe{"dollar_run_at_end", "dollar_run_at_end", {}, meios::diagnostic_code::expression_error, "invalid expression: $$"},
        agreement_probe{"dollar_run_inside_expression", "dollar_run_inside_expression", {}, meios::diagnostic_code::expression_error, "invalid expression: $$'"},
        agreement_probe{"include_escaped_find", "include_escaped_find", {}, meios::diagnostic_code::unresolved_include, "No such file or directory", "include_escaped_find.d"},
        agreement_probe{"dollar_run_in_argument_default", "dollar_run_in_argument_default", {}, meios::diagnostic_code::expression_error, "invalid expression: $$x"},
        agreement_probe{"dollar_run_in_unused_argument_default", "dollar_run_in_unused_argument_default", {}, meios::diagnostic_code::expression_error, "invalid expression: $$x"},
        agreement_probe{
                "dollar_run_in_redeclared_argument_default", "dollar_run_in_redeclared_argument_default", {}, meios::diagnostic_code::expression_error, "invalid expression: $$x"},
        agreement_probe{"argument_redeclared_after_refusing_default",
                        "argument_redeclared_after_refusing_default",
                        {},
                        meios::diagnostic_code::undefined_property,
                        "name 'nope' is not defined"},
        agreement_probe{"argument_redeclared_after_expression_default", "argument_redeclared_after_expression_default", {}},
        agreement_probe{"argument_declared_in_skipped_branch", "argument_declared_in_skipped_branch", {}},
        agreement_probe{"argument_declared_in_uncalled_macro", "argument_declared_in_uncalled_macro", {}},
        agreement_probe{"argument_declared_in_include_first", "argument_declared_in_include_first", {}},
        agreement_probe{"dollar_run_after_undefined_name", "dollar_run_after_undefined_name", {}, meios::diagnostic_code::expression_error, "invalid expression: $$x"},
        agreement_probe{"dollar_run_after_unset_argument", "dollar_run_after_unset_argument", {}, meios::diagnostic_code::expression_error, "invalid expression: $$x"},
        agreement_probe{"dollar_run_after_division_by_zero", "dollar_run_after_division_by_zero", {}, meios::diagnostic_code::expression_error, "invalid expression: $$x"},
        agreement_probe{
                "unterminated_span_after_undefined_name", "unterminated_span_after_undefined_name", {}, meios::diagnostic_code::unterminated_substitution, "invalid expression: ${x"},
        agreement_probe{"macro_params_only_whitespace", "macro_params_only_whitespace", {}, meios::diagnostic_code::xacro_structural_error, "list index out of range"},
        agreement_probe{"macro_params_only_whitespace_never_reached", "macro_params_only_whitespace_never_reached", {}},
        agreement_probe{"macro_default_reads_supplied_parameter", "macro_default_reads_supplied_parameter", {}},
        agreement_probe{"argument_default_reads_itself_before_redeclaration",
                        "argument_default_reads_itself_before_redeclaration",
                        {},
                        meios::diagnostic_code::unresolved_arg,
                        "Undefined substitution argument a"},
};

}

#endif

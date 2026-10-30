#ifndef HPP_GUARD_MEIOS_TESTS_GRAMMAR_DOCUMENT_PROBES_H
#define HPP_GUARD_MEIOS_TESTS_GRAMMAR_DOCUMENT_PROBES_H

#include "differential_document.h"

#include <array>

namespace differential
{

#ifdef MEIOS_TEST_HAS_YAML

// Spelled exactly as grammar_probes.py spells them, at the same positions.
inline const std::array<document_probe, 70> grammar_document_probes{
        document_probe{false, "include_optional_missing_file"},
        document_probe{false, "macro_called_by_name_attribute"},
        document_probe{false, "element_named_by_attribute"},
        document_probe{false, "attribute_set_on_parent"},
        document_probe{false, "property_name_from_expression"},
        document_probe{false, "property_name_not_identifier"},
        document_probe{false, "property_name_double_underscore"},
        document_probe{false, "root_target_namespace"},
        document_probe{
                false, "include_find_answering_root_only", false, false, {}, false, {}, {"include_find_answering_root_only.d/r1", "include_find_answering_root_only.d/r2"}, false},
        document_probe{false, "macro_params_unicode_space_between"},
        document_probe{false, "argument_lent_from_false_conditional"},
        document_probe{false, "argument_lent_from_uncalled_macro"},
        document_probe{false, "include_missing_element_before_parent"},
        document_probe{false, "include_missing_element_before_parent_nested"},
        document_probe{false, "include_find_missing_element_before_parent", false, false, {}, false, {}, {"include_find_missing_element_before_parent.d"}, false},
        document_probe{false, "include_held_find_in_property", false, false, {}, false, {}, {"include_held_find_in_property.d"}, false},
        document_probe{false, "include_held_find_in_argument", false, false, {}, false, {}, {"include_held_find_in_argument.d"}, false},
        document_probe{false, "include_held_find_directory", false, false, {}, false, {}, {"include_held_find_directory.d"}, false},
        document_probe{false, "include_find_composed_reads_neighbor", false, false, {}, false, {}, {"include_find_composed_reads_neighbor.d"}, false},
        document_probe{false, "include_find_trailing_separator", false, false, {}, false, {}, {"include_find_trailing_separator.d"}, false},
        document_probe{false, "cwd_command", false, false, {}, false, {{"here", "@STAGE@"}}, {}, true},
        document_probe{false, "eval_command_padded"},
        document_probe{false, "eval_command_empty"},
        document_probe{false, "eval_command_blank"},
        document_probe{false, "env_command_two_words"},
        document_probe{false, "optenv_command_fallback_words"},
        document_probe{false, "empty_command"},
        document_probe{true, "yaml_empty_scalar_key"},
        document_probe{false, "property_quoted_value"},
        document_probe{false, "property_padded_number"},
        document_probe{false, "property_signed_number"},
        document_probe{false, "property_unicode_digit"},
        document_probe{false, "property_infinity_spelling"},
        document_probe{false, "macro_argument_literal_reading"},
        document_probe{false, "conditional_real_one"},
        document_probe{false, "conditional_upper_true"},
        document_probe{false, "conditional_empty"},
        document_probe{false, "conditional_quoted_text"},
        document_probe{false, "property_without_name"},
        document_probe{false, "macro_without_name"},
        document_probe{false, "conditional_without_value"},
        document_probe{false, "unless_without_value"},
        document_probe{false, "include_without_filename"},
        document_probe{false, "macro_named_call"},
        document_probe{false, "macro_name_with_dot"},
        document_probe{false, "macro_name_with_xacro_prefix"},
        document_probe{false, "comment_kept_in_output", false, false, {}, false, {}, {}, false, true},
        document_probe{false, "xacro_attribute_removed"},
        document_probe{false, "xacro_attribute_never_evaluated"},
        document_probe{false, "included_root_namespace_lifted"},
        document_probe{false, "macro_namespace_lifted"},
        document_probe{false, "cdata_kept_verbatim"},
        document_probe{false, "attribute_under_conditional"},
        document_probe{false, "attribute_under_macro_body"},
        document_probe{false, "attribute_under_included_root"},
        document_probe{false, "attribute_under_block_wrapper"},
        document_probe{false, "attribute_under_block_element"},
        document_probe{false, "attribute_on_document_root"},
        document_probe{false, "element_renamed_to_conditional"},
        document_probe{false, "element_renamed_to_macro_call"},
        document_probe{false, "comment_evaluated_under_eval_comments", false, false, {}, false, {}, {}, false, true},
        document_probe{false, "comment_before_xacro_element_removed", false, false, {}, false, {}, {}, false, true},
        document_probe{false, "processing_instruction_kept", false, false, {}, false, {}, {}, false, true},
        document_probe{false, "include_glob_matching_nothing"},
        document_probe{false, "include_namespace_not_text"},
        document_probe{false, "comment_evaluation_switched_off", false, false, {}, false, {}, {}, false, true},
        document_probe{false, "find_without_package_index"},
        document_probe{false, "property_chain_deeper_than_depth_ceiling"},
        document_probe{false, "include_namespace_read_as_value"},
        document_probe{false, "include_namespace_reads_mathematics_name"},
};

#endif

}

#endif

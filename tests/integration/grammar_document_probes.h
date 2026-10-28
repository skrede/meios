#ifndef HPP_GUARD_MEIOS_TESTS_GRAMMAR_DOCUMENT_PROBES_H
#define HPP_GUARD_MEIOS_TESTS_GRAMMAR_DOCUMENT_PROBES_H

#include "differential_document.h"

#include <array>

namespace differential
{

#ifdef MEIOS_TEST_HAS_YAML

// Spelled exactly as grammar_probes.py spells them, at the same positions.
inline const std::array<document_probe, 43> grammar_document_probes{
        document_probe{false, "property_evaluated_at_first_read"}, document_probe{false, "property_circular_definition"},
        document_probe{false, "property_self_reference"},          document_probe{false, "property_never_read_undefined_name"},
        document_probe{false, "property_never_read_dollar_run"},   document_probe{false, "property_default_reads_later_property"},
        document_probe{false, "property_removed_then_read"},       document_probe{false, "property_value_and_remove_together"},
        document_probe{false, "include_namespace_bare_name"},      document_probe{false, "include_namespace_dotted_property"},
        document_probe{false, "include_namespace_dotted_macro"},   document_probe{false, "include_optional_missing_file"},
        document_probe{false, "macro_called_by_name_attribute"},   document_probe{false, "element_named_by_attribute"},
        document_probe{false, "attribute_set_on_parent"},          document_probe{false, "property_block_body_inserted"},
        document_probe{false, "property_name_from_expression"},    document_probe{false, "property_name_not_identifier"},
        document_probe{false, "property_name_double_underscore"},  document_probe{false, "root_target_namespace"},
        document_probe{false, "include_find_answering_root_only", false, false, {}, false, {}, {"include_find_answering_root_only.d/r1", "include_find_answering_root_only.d/r2"}, false},
        document_probe{false, "macro_params_unicode_space_between"},
        document_probe{false, "argument_lent_from_false_conditional"},
        document_probe{false, "argument_lent_from_uncalled_macro"},
        document_probe{false, "macro_parameter_twice_read_after_call"},
        document_probe{false, "macro_parameter_twice_over_outer"},
        document_probe{false, "property_exact_span_open_span_unread"},
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
};

#endif

}

#endif

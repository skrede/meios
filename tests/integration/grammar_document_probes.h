#ifndef HPP_GUARD_MEIOS_TESTS_GRAMMAR_DOCUMENT_PROBES_H
#define HPP_GUARD_MEIOS_TESTS_GRAMMAR_DOCUMENT_PROBES_H

#include "differential_document.h"

#include <array>

namespace differential
{

#ifdef MEIOS_TEST_HAS_YAML

// Spelled exactly as grammar_probes.py spells them, at the same positions.
inline constexpr std::array<document_probe, 20> grammar_document_probes{
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
};

#endif

}

#endif

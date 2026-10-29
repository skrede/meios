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
};

}

#endif

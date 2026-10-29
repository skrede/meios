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
};

}

#endif

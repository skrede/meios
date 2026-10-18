#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_AGREEMENT_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_AGREEMENT_H

#include "differential_xml.h"

#include "../corpus_record.h"
#include "../native_expansion.h"

#include <meios/urdf.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <string_view>

#ifndef MEIOS_XACRO_PROBE_DIR
    #define MEIOS_XACRO_PROBE_DIR ""
#endif

namespace differential
{

// A minimized binding document both sides must render alike, under the caller arguments the
// binding needs. Spelled exactly as differential.py's own tuple spells it, so an id names the same
// fixture and the same rendered file whichever side wrote it.
struct agreement_probe
{
    std::string_view id;
    std::string_view fixture;
    std::map<std::string, std::string> args;
};

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
        agreement_probe{"argument_by_bare_name", "argument_by_bare_name", {}},
        agreement_probe{"argument_command_reads_no_property", "argument_command_reads_no_property", {}},
        agreement_probe{"inherited_default_skips_argument", "inherited_default_skips_argument", {}},
        agreement_probe{"inherited_argument_refuses", "inherited_argument_refuses", {}},
};

// Document-level agreement, on the project's own normalizer applied to both sides -- the same
// normalizer, and the same structural comparison layered on its output, that the expression arm
// already runs. Comparing unnormalized text would make attribute order and insignificant
// whitespace a difference; comparing the model alone leaves every element the fact vocabulary has
// no term for unasserted.
inline void require_same_document(const std::string &upstream_text, const std::string &rendered)
{
    const std::string meios_canon    = meios::canonical_xml(rendered);
    const std::string upstream_canon = meios::canonical_xml(upstream_text);
    std::string mismatch;
    if(!canonical_matches(upstream_canon, meios_canon, mismatch))
        FAIL("the rendered documents disagree: " << mismatch);
}

// The caller arguments reach both the load a consumer runs and the expansion the corpus arm
// compares, so a probe cannot agree by seeding a table the load path never writes.
inline void compare_agreement_probe(const agreement_probe &one, const std::string &renders_dir, std::vector<std::string> &seen)
{
    const std::string id(one.id);
    seen.push_back(id);
    INFO("agreement probe " << id);
    const std::filesystem::path scratch = render_path(renders_dir, id, ".xml");
    REQUIRE(std::filesystem::exists(scratch));
    const std::string upstream_text = slurp(scratch);
    const bool upstream_refused     = refused(upstream_text);
    const corpus::document doc{std::filesystem::path{MEIOS_XACRO_PROBE_DIR} / (std::string(one.fixture) + ".xacro"), one.args, {}, corpus::evaluator::native, {}};
    meios::load_options opts;
    opts.args = one.args;
    CHECK(meios::load(doc.path, opts).has_value() == !upstream_refused);
    const std::optional<meios::expansion> rendered = corpus::expanded(doc);
    REQUIRE(rendered.has_value() == !upstream_refused);
    if(!upstream_refused)
        require_same_document(upstream_text, rendered->document);
}

}

#endif

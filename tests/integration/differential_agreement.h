#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_AGREEMENT_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_AGREEMENT_H

#include "differential_xml.h"

#include "../corpus_record.h"
#include "../native_expansion.h"

#include <meios/urdf.h>
#include <meios/xacro.h>

#include <meios/bundle/flatten.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <optional>
#include <filesystem>
#include <string_view>

#ifndef MEIOS_XACRO_PROBE_DIR
    #define MEIOS_XACRO_PROBE_DIR ""
#endif

namespace differential
{

// A minimized binding document both sides must render alike, under the caller arguments the
// binding needs. The first three fields are spelled exactly as differential.py's own tuple spells
// them, so an id names the same fixture and the same rendered file whichever side wrote it. A
// probe both sides refuse also names the cause each side must give.
struct agreement_probe
{
    std::string_view id;
    std::string_view fixture;
    std::map<std::string, std::string> args;
    std::optional<meios::diagnostic_code> refuses;
    std::string_view upstream_cause;
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

inline std::filesystem::path agreement_fixture(const agreement_probe &one)
{
    return std::filesystem::path{MEIOS_XACRO_PROBE_DIR} / (std::string(one.fixture) + ".xacro");
}

// The model the load built, written back out by the serializer the command-line flatten uses.
inline std::string loaded_document(const meios::load_result &loaded)
{
    std::ostringstream out;
    meios::log_sink silent;
    meios::flatten(loaded.robot, out, silent);
    return out.str();
}

// A refusal agrees only on its cause, so a fixture gone from both tables, a malformed one, or a
// refusal under an unrelated code cannot pass as agreement.
inline void require_same_refusal(const agreement_probe &one, const std::string &upstream_text, const meios::load_error &error)
{
    REQUIRE(one.refuses.has_value());
    CHECK(error.code == *one.refuses);
    CHECK(upstream_text.find(one.upstream_cause) != std::string::npos);
}

// The load a consumer runs is what is compared, under the caller arguments it would hand that
// load, so a probe cannot agree by seeding a table the load path never writes. The expansion the
// corpus arm compares is held to the same render beside it.
inline void compare_agreement_probe(const agreement_probe &one, const std::string &renders_dir, std::vector<std::string> &seen)
{
    const std::string id(one.id);
    seen.push_back(id);
    INFO("agreement probe " << id);
    const std::filesystem::path scratch = render_path(renders_dir, id, ".xml");
    REQUIRE(std::filesystem::exists(scratch));
    REQUIRE(std::filesystem::exists(agreement_fixture(one)));
    const std::string upstream_text = slurp(scratch);
    meios::load_options opts;
    opts.args = one.args;
    const meios::expected<meios::load_result, meios::load_error> loaded = meios::load(agreement_fixture(one), opts);
    REQUIRE(loaded.has_value() == !refused(upstream_text));
    if(!loaded)
        return require_same_refusal(one, upstream_text, loaded.error());
    REQUIRE_FALSE(one.refuses.has_value());
    require_same_document(upstream_text, loaded_document(*loaded));
    const std::optional<meios::expansion> rendered = corpus::expanded(corpus::document{agreement_fixture(one), one.args, {}, corpus::evaluator::native, {}});
    REQUIRE(rendered.has_value());
    require_same_document(upstream_text, rendered->document);
}

}

#endif

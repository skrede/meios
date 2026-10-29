#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_AGREEMENT_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_AGREEMENT_H

#include "agreement_probes.h"
#include "differential_xml.h"
#include "grammar_agreement_probes.h"

#include "../corpus_record.h"
#include "../refusal_causes.h"
#include "../native_expansion.h"

#include <meios/urdf.h>
#include <meios/xacro.h>

#include <meios/bundle/flatten.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <sstream>
#include <optional>
#include <filesystem>

#ifndef MEIOS_XACRO_PROBE_DIR
    #define MEIOS_XACRO_PROBE_DIR ""
#endif

namespace differential
{

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

inline std::filesystem::path agreement_root(const agreement_probe &one)
{
    return one.root.empty() ? std::filesystem::path{} : std::filesystem::path{MEIOS_XACRO_PROBE_DIR} / one.root;
}

// The model the load built, written back out by the serializer the command-line flatten uses.
inline std::string loaded_document(const meios::load_result &loaded)
{
    std::ostringstream out;
    meios::log_sink silent;
    meios::flatten(loaded.robot, out, silent);
    return out.str();
}

inline corpus::document agreement_document(const agreement_probe &one)
{
    return corpus::document{agreement_fixture(one), one.args, agreement_root(one), corpus::evaluator::native, {}};
}

// A refusal agrees only on its cause, so a fixture gone from both tables, a malformed one, or a
// refusal under an unrelated code cannot pass as agreement. Upstream's cause is held to the same
// code through the cause table, and its wording to the stem the probe names.
inline void require_same_refusal(const agreement_probe &one, const std::string &upstream_text, meios::diagnostic_code code)
{
    REQUIRE(one.refuses.has_value());
    CHECK(code == *one.refuses);
    CHECK(upstream_text.find(one.upstream_cause) != std::string::npos);
    const upstream_refusal cause                       = parse_refusal(upstream_text);
    const std::optional<meios::diagnostic_code> mapped = mapped_cause(cause.outer, cause.inner, cause.line);
    INFO("upstream refused with " << cause.outer << ' ' << cause.inner << ": " << cause.line << ", mapped to "
                                  << (mapped ? meios::to_string(*mapped) : "no code") << ", expected " << meios::to_string(*one.refuses));
    CHECK(mapped == one.refuses);
}

// The load a consumer runs is what is compared, under the caller arguments it would hand that
// load, so a probe cannot agree by seeding a table the load path never writes. Answers whether the
// load went far enough for its expansion to be compared too.
inline bool require_same_load(const agreement_probe &one, const std::string &upstream_text)
{
    meios::load_options opts;
    opts.args     = one.args;
    opts.includes = one.includes;
    if(!one.root.empty())
        opts.package_roots.push_back(agreement_root(one));
    const meios::expected<meios::load_result, meios::load_error> loaded = meios::load(agreement_fixture(one), opts);
    REQUIRE(loaded.has_value() == !refused(upstream_text));
    if(!loaded)
    {
        require_same_refusal(one, upstream_text, loaded.error().code);
        return false;
    }
    REQUIRE_FALSE(one.refuses.has_value());
    require_same_document(upstream_text, loaded_document(*loaded));
    return true;
}

inline void require_same_expansion(const agreement_probe &one, const std::string &upstream_text)
{
    const meios::expected<meios::expansion, meios::expansion_error> rendered = corpus::expansion_of(agreement_document(one), one.includes);
    REQUIRE(rendered.has_value() == !refused(upstream_text));
    if(!rendered)
        return require_same_refusal(one, upstream_text, rendered.error().code);
    REQUIRE_FALSE(one.refuses.has_value());
    require_same_document(upstream_text, rendered->document);
}

inline void compare_agreement_probe(const agreement_probe &one, const std::string &renders_dir, std::vector<std::string> &seen)
{
    const std::string id(one.id);
    seen.push_back(id);
    INFO("agreement probe " << id);
    const std::filesystem::path scratch = render_path(renders_dir, id, ".xml");
    REQUIRE(std::filesystem::exists(scratch));
    REQUIRE(std::filesystem::exists(agreement_fixture(one)));
    const std::string upstream_text = slurp(scratch);
    if(one.expansion_only || require_same_load(one, upstream_text))
        require_same_expansion(one, upstream_text);
}

inline void compare_agreement_table(const std::vector<agreement_probe> &table, const std::string &renders_dir, std::vector<std::string> &seen)
{
    for(const agreement_probe &one : table)
        compare_agreement_probe(one, renders_dir, seen);
}

}

#endif

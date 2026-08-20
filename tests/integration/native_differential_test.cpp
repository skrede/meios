#include "differential_xml.h"
#include "differential_seed.h"

#include "../model_facts.h"
#include "../corpus_record.h"
#include "../native_expansion.h"
#include "../differential_verdict.h"

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <string_view>

#ifndef MEIOS_DIFFERENTIAL_RENDERS_DIR
    #define MEIOS_DIFFERENTIAL_RENDERS_DIR ""
#endif

namespace
{

// Populated only by the CI job that ran differential.py first; a plain MEIOS_FETCH_CORPUS
// configure with no scratch renders still builds this target and skips its body cleanly.
bool renders_available()
{
    return std::string_view(MEIOS_DIFFERENTIAL_RENDERS_DIR).size() > 0;
}

void skip_without_renders()
{
    WARN("MEIOS_DIFFERENTIAL_RENDERS_DIR is empty; skipping the live differential");
}

meios::load_options native_options(const corpus::document &doc)
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    opts.materials  = meios::material_policy::warn;
    opts.args       = doc.args;
    if(!doc.package_root.empty())
        opts.package_roots.push_back(doc.package_root);
    return opts;
}

// A fresh upstream render has no xacro: tags left, so it loads with the argument set already
// applied to it rather than supplied again.
meios::load_options rendered_options(const corpus::document &doc)
{
    meios::load_options opts = native_options(doc);
    opts.args.clear();
    return opts;
}

// Both paths meet at the model: upstream's own render feeds through the same meios::load()
// entry point, and is held to the same measured facts.
void check_fresh_render(const corpus::document &doc, const std::filesystem::path &scratch,
                        const std::vector<oracle::row> &facts_record)
{
    const meios::expected<meios::load_result, meios::load_error> fresh =
        meios::load(scratch, rendered_options(doc));
    REQUIRE(fresh.has_value());
    facts::check_model(facts_record, fresh->robot, doc.package_root);
}

// A canonical disagreement is tolerable only where the manifest already records it, matched by
// exact recorded text rather than by id alone.
void require_reviewed_divergence(const std::vector<oracle::row> &manifest, const std::string &id,
                                 const std::string &upstream_text, const std::string &rendered)
{
    const std::string meios_canon =
        meios::canonical_xml(differential::escaped_fragment(rendered));
    const std::string upstream_canon = meios::canonical_xml(upstream_text);
    std::string mismatch;
    if(differential::canonical_matches(upstream_canon, meios_canon, mismatch))
        return;
    const std::string upstream_value = differential::attribute_v(upstream_text);
    std::string manifest_detail;
    if(!differential::matches_manifest(manifest, id, upstream_value, rendered, manifest_detail))
        FAIL(manifest_detail);
}

void compare_expression_case(const oracle::row &row, const std::vector<oracle::row> &manifest,
                             std::vector<std::string> &seen)
{
    REQUIRE(row.fields.size() == 4);
    const std::string &id = row.fields[0];
    seen.push_back(id);
    INFO("case " << id << ": " << row.fields[1]);
    const std::filesystem::path scratch =
        differential::render_path(MEIOS_DIFFERENTIAL_RENDERS_DIR, id, ".xml");
    REQUIRE(std::filesystem::exists(scratch));
    const std::string upstream_text = differential::slurp(scratch);
    const bool upstream_refused = differential::refused(upstream_text);
    const differential::outcome ran = differential::evaluate_seeded(row.fields[1]);
    CHECK(ran.failed == upstream_refused);
    if(ran.failed || upstream_refused)
        return;
    require_reviewed_divergence(manifest, id, upstream_text, ran.rendered);
}

// Document-level agreement, on the project's own normalizer applied to both sides -- the same
// normalizer, and the same structural comparison layered on its output, that the expression arm
// already runs. Comparing unnormalized text would make attribute order and insignificant
// whitespace a difference; comparing the model alone leaves every element the fact vocabulary has
// no term for unasserted.
void require_same_document(const std::string &upstream_text, const std::string &rendered)
{
    const std::string meios_canon    = meios::canonical_xml(rendered);
    const std::string upstream_canon = meios::canonical_xml(upstream_text);
    std::string mismatch;
    if(!differential::canonical_matches(upstream_canon, meios_canon, mismatch))
        FAIL("the rendered documents disagree: " << mismatch);
}

void compare_corpus_document(const corpus::document &doc, std::vector<std::string> &seen)
{
    if(doc.backend != corpus::evaluator::native)
        return;
    const std::string id = corpus::document_key(doc);
    const std::string record_file = corpus::record_for(doc);
    REQUIRE_FALSE(record_file.empty());
    seen.push_back(id);
    INFO("document: " << doc.path.string() << " (" << id << ")");
    const std::filesystem::path scratch =
        differential::render_path(MEIOS_DIFFERENTIAL_RENDERS_DIR, id, ".xml");
    REQUIRE(std::filesystem::exists(scratch));
    const std::string upstream_text = differential::slurp(scratch);
    const bool upstream_refused = differential::refused(upstream_text);
    const meios::expected<meios::load_result, meios::load_error> own =
        meios::load(doc.path, native_options(doc));
    CHECK(own.has_value() == !upstream_refused);
    if(!own.has_value() || upstream_refused)
        return;
    const std::vector<oracle::row> facts_record = oracle::load_rows(record_file);
    facts::check_model(facts_record, own->robot, doc.package_root);
    check_fresh_render(doc, scratch, facts_record);
    const std::optional<meios::expansion> rendered = corpus::expanded(doc);
    REQUIRE(rendered.has_value());
    require_same_document(upstream_text, rendered->document);
}

}

// One test case, not two, so the named expected-comparison inventory is checked once
// against the union of both tiers' verdicts -- a comparison this target owes but skips shows up
// as a missing inventory entry, not as a false negative in whichever tier's own loop ran first.
TEST_CASE("fresh upstream renders agree with meios on category, structure and facts",
          "[native_differential]")
{
    if(!renders_available())
        return skip_without_renders();

    const std::vector<oracle::row> inventory = oracle::load_rows("differential_inventory.cases");
    const std::vector<oracle::row> manifest = oracle::load_rows("differential_divergences.cases");
    std::vector<std::string> seen;
    for(const oracle::row &row : oracle::load_rows("expressions.cases"))
        compare_expression_case(row, manifest, seen);
    for(const corpus::document &doc : corpus::documents(MEIOS_CORPUS_DOCUMENTS))
        compare_corpus_document(doc, seen);
    const std::optional<std::string> missing = differential::first_missing_verdict(inventory, seen);
    if(missing)
        FAIL("no differential verdict was produced for inventory entry '" << *missing << "'");
}

// Registered only where the auxiliary-document module is built, because one of the probes reads
// a document: a build with no reader could observe that divergence only by asserting it from the
// manifest it is meant to police.
#ifdef MEIOS_TEST_HAS_YAML

TEST_CASE("the divergence manifest matches the measured divergences and no others",
          "[native_differential]")
{
    if(!renders_available())
        return skip_without_renders();

    const std::vector<oracle::row> manifest = oracle::load_rows("differential_divergences.cases");
    std::vector<std::tuple<std::string, std::string, std::string>> observed;
    for(const differential::probe &one : differential::divergence_probes)
    {
        const std::filesystem::path scratch =
            differential::render_path(MEIOS_DIFFERENTIAL_RENDERS_DIR, std::string(one.id), ".txt");
        REQUIRE(std::filesystem::exists(scratch));
        const std::string rendered = differential::trimmed(differential::slurp(scratch));
        // A refusing render carries upstream's own wording after the verdict, which no manifest
        // column can hold: a tab is the column separator. Both sides therefore record the
        // verdict, and the wording of a refusal is what expressions.cases exists to carry.
        const std::string upstream_text =
            differential::refused(rendered) ? std::string("REFUSED") : rendered;
        INFO("probe " << one.id << ": " << one.expression);
        const std::string meios_value = differential::observed_value(one);
        REQUIRE(upstream_text != meios_value);
        std::string detail;
        if(!differential::matches_manifest(manifest, std::string(one.id), upstream_text,
                                           meios_value, detail))
            FAIL(detail);
        observed.emplace_back(one.id, upstream_text, meios_value);
    }
    for(const std::string &stale : differential::stale_manifest_entries(manifest, observed))
        FAIL("divergence manifest entry '" << stale << "' no longer reproduces");
}

#endif

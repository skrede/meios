#include "differential_seed.h"
#include "differential_xml.h"

#include "../model_facts.h"
#include "../corpus_record.h"
#include "../differential_verdict.h"

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <tuple>
#include <vector>
#include <utility>
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
    const std::string meios_canon =
        meios::canonical_xml(differential::escaped_fragment(ran.rendered));
    const std::string upstream_canon = meios::canonical_xml(upstream_text);
    std::string mismatch;
    if(differential::canonical_matches(upstream_canon, meios_canon, mismatch))
        return;
    const std::string upstream_value = differential::attribute_v(upstream_text);
    std::string manifest_detail;
    if(!differential::matches_manifest(manifest, id, upstream_value, ran.rendered, manifest_detail))
        FAIL(manifest_detail);
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

    // "The two paths meet at the model" (D-13): upstream's own fresh render, with no xacro:
    // tags left, feeds through the same meios::load() entry point.
    meios::load_options fresh_opts;
    fresh_opts.on_missing = meios::missing_asset::warn;
    fresh_opts.materials  = meios::material_policy::warn;
    if(!doc.package_root.empty())
        fresh_opts.package_roots.push_back(doc.package_root);
    const meios::expected<meios::load_result, meios::load_error> fresh =
        meios::load(scratch, fresh_opts);
    REQUIRE(fresh.has_value());
    facts::check_model(facts_record, fresh->robot, doc.package_root);
}

}

// One test case, not two, so the named expected-comparison inventory (D-06) is checked once
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

TEST_CASE("the divergence manifest matches the measured and/or divergences and no others",
          "[native_differential]")
{
    if(!renders_available())
        return skip_without_renders();

    const std::vector<oracle::row> manifest = oracle::load_rows("differential_divergences.cases");
    const std::pair<std::string, std::string> probes[] = { { "div_or_operand", "1 or 2" },
                                                            { "div_and_operand", "2 and 3" } };
    std::vector<std::tuple<std::string, std::string, std::string>> observed;
    for(const std::pair<std::string, std::string> &probe : probes)
    {
        const std::filesystem::path scratch =
            differential::render_path(MEIOS_DIFFERENTIAL_RENDERS_DIR, probe.first, ".txt");
        REQUIRE(std::filesystem::exists(scratch));
        std::string upstream_text = differential::slurp(scratch);
        while(!upstream_text.empty()
              && (upstream_text.back() == '\n' || upstream_text.back() == '\r'))
            upstream_text.pop_back();
        INFO("probe " << probe.first << ": " << probe.second);
        const differential::outcome ran = differential::evaluate_bare(probe.second);
        const std::string meios_value = ran.failed ? "REFUSED" : ran.rendered;
        REQUIRE(upstream_text != meios_value);
        std::string detail;
        if(!differential::matches_manifest(manifest, probe.first, upstream_text, meios_value,
                                           detail))
            FAIL(detail);
        observed.emplace_back(probe.first, upstream_text, meios_value);
    }
    for(const std::string &stale : differential::stale_manifest_entries(manifest, observed))
        FAIL("divergence manifest entry '" << stale << "' no longer reproduces");
}

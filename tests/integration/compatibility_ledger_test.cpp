#include "../corpus_record.h"
#include "../corpus_listfile.h"
#include "../native_expansion.h"

#include "../unit/oracle_records.h"
#include "../unit/construct_probe.h"

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <algorithm>

namespace
{

enum ledger_column
{
    entry_point,
    document_path,
    revision,
    arguments,
    upstream_result,
    native_result,
    exercised_constructs,
    reviewed_divergence,
    ledger_column_count
};

std::vector<oracle::row> ledger()
{
    return oracle::load_rows("compatibility_ledger.cases");
}

std::vector<std::string> spellings_in(const std::string &column)
{
    std::vector<std::string> found;
    for(std::size_t at = 0; at < column.size();)
    {
        const std::size_t stop = std::min(column.find(',', at), column.size());
        found.push_back(column.substr(at, stop - at));
        at = column.find_first_not_of(' ', stop + 1);
        if(at == std::string::npos)
            break;
    }
    return found;
}

// The vocabulary is what a spelling is read against, and it answers an unrecognized one with
// nothing, so a column cannot drift into prose without failing here.
meios::construct_set set_of(const std::vector<std::string> &spellings)
{
    meios::construct_set out;
    for(const std::string &one : spellings)
    {
        INFO("construct spelling: " << one);
        const std::optional<meios::evaluator_construct> found = meios::construct_from_name(one);
        REQUIRE(found.has_value());
        out.mark(*found);
    }
    return out;
}

std::string shape_text(std::size_t links, std::size_t joints)
{
    return "links=" + std::to_string(links) + " joints=" + std::to_string(joints);
}

// Every entry point the corpus records for the built-in evaluator, keyed exactly as the
// measurement records are keyed. Read from the listfile rather than from the configured document
// set so a row is paired against what the corpus *is*, not against what one build option selected.
std::vector<std::string> pinned_keys()
{
    std::vector<std::string> keys;
    for(const std::vector<std::string> &record :
        corpus::recorded_documents(corpus::listfile_text("corpus_documents.cmake")))
    {
        REQUIRE(record.size() == 4);
        const corpus::document doc{ record[0], corpus::arg_map(record[1]), record[3],
                                    corpus::classify(record[2]), record[2] };
        if(doc.backend == corpus::evaluator::native)
            keys.push_back(corpus::document_key(doc));
    }
    return keys;
}

struct observation
{
    meios::construct_set exercised;
    std::string shape;
};

// The construct set comes from the expansion, which is where the evaluation happens; the shape
// comes from the load proper, so the recorded native result is a model rather than a count taken
// off the flattened text.
observation observe(const corpus::document &doc)
{
    const std::optional<meios::expansion> out = corpus::expanded(doc);
    REQUIRE(out.has_value());

    meios::load_options opts;
    opts.on_missing    = meios::missing_asset::warn;
    opts.materials     = meios::material_policy::warn;
    opts.args          = doc.args;
    if(!doc.package_root.empty())
        opts.package_roots.push_back(doc.package_root);
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(doc.path, opts);
    REQUIRE(loaded.has_value());
    return { out->exercised, shape_text(loaded->robot.links.size(), loaded->robot.joints.size()) };
}

}

TEST_CASE("every pinned entry point has one ledger row and every ledger row names one", "[ledger]")
{
    const std::vector<std::string> pinned = pinned_keys();
    const std::vector<oracle::row> rows = ledger();
    REQUIRE_FALSE(pinned.empty());

    for(const std::string &key : pinned)
    {
        INFO("pinned entry point: " << key);
        CHECK(oracle::lookup(rows, key).has_value());
    }
    for(const oracle::row &one : rows)
    {
        REQUIRE(one.fields.size() == ledger_column_count);
        INFO("ledger row: " << one.fields[entry_point]);
        CHECK(std::ranges::find(pinned, one.fields[entry_point]) != pinned.end());
    }
    CHECK(rows.size() == pinned.size());
}

TEST_CASE("a ledger row's constructs are read against the closed vocabulary", "[ledger]")
{
    for(const oracle::row &one : ledger())
    {
        INFO("ledger row: " << one.fields[entry_point]);
        set_of(spellings_in(one.fields[exercised_constructs]));
    }
    CHECK(set_of({ "alias", "unit-tag" }) == set_of({ "unit-tag", "alias" }));
    CHECK(meios::construct_from_name("unit_tag") == std::nullopt);
}

// The divergence column is an assertion in both directions, the way the divergence manifest
// beside this record is: an empty column claims the two results agree, and a filled one claims
// they do not, so a divergence recorded after it stopped reproducing fails here rather than
// quietly excusing a comparison.
TEST_CASE("a ledger row's divergence column is checked in both directions", "[ledger]")
{
    for(const oracle::row &one : ledger())
    {
        INFO("ledger row: " << one.fields[entry_point]);
        const bool agree = one.fields[upstream_result] == one.fields[native_result];
        CHECK(agree == one.fields[reviewed_divergence].empty());
    }
}

// The pairing runs through the key and nothing else, which is what lets two entry points
// exercising the same constructs each keep their own row.
TEST_CASE("entry points exercising the same constructs still pair one to one", "[ledger]")
{
    const std::vector<oracle::row> rows = ledger();
    std::vector<std::string> keys;
    std::size_t shared = 0;
    for(std::size_t one = 0; one < rows.size(); ++one)
    {
        for(std::size_t other = 0; other < rows.size(); ++other)
            if(one != other
               && rows[one].fields[exercised_constructs]
                      == rows[other].fields[exercised_constructs])
                ++shared;
        CHECK(std::ranges::find(keys, rows[one].fields[entry_point]) == keys.end());
        keys.push_back(rows[one].fields[entry_point]);
    }
    INFO("rows sharing a construct set with another row: " << shared);
    CHECK(keys.size() == rows.size());
}

TEST_CASE("each ledger row names the immutable revision the corpus fetches", "[ledger]")
{
    const std::string listfile = corpus::listfile_text("corpus.cmake");
    for(const oracle::row &one : ledger())
    {
        const std::string &named = one.fields[revision];
        const std::size_t at = named.find('@');
        INFO("revision: " << named);
        REQUIRE(at != std::string::npos);
        CHECK(listfile.find("NAME " + named.substr(0, at)) != std::string::npos);
        CHECK(listfile.find("SHA256=" + named.substr(at + 1)) != std::string::npos);
    }
}

#ifdef MEIOS_TEST_HAS_YAML

TEST_CASE("each pinned entry point exercises and loads to what its ledger row records", "[ledger]")
{
    const std::vector<oracle::row> rows = ledger();
    std::size_t compared = 0;
    for(const corpus::document &doc : corpus::documents(MEIOS_CORPUS_DOCUMENTS))
    {
        if(doc.backend != corpus::evaluator::native)
            continue;
        const std::string key = corpus::document_key(doc);
        INFO("entry point: " << key << " (" << doc.path.string() << ")");
        const std::optional<oracle::row> recorded = oracle::lookup(rows, key);
        REQUIRE(recorded.has_value());
        const observation got = observe(doc);
        CHECK(construct_probe::listed(got.exercised)
              == construct_probe::listed(
                  set_of(spellings_in(recorded->fields[exercised_constructs]))));
        CHECK(got.shape == recorded->fields[native_result]);
        ++compared;
    }
    CHECK(compared == rows.size());
}

#endif

#include "compatibility_ledger.h"

#include "../corpus_record.h"
#include "../corpus_listfile.h"

#include "../unit/oracle_records.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <algorithm>

namespace
{

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

}

TEST_CASE("every pinned entry point has one ledger row and every ledger row names one", "[ledger]")
{
    const std::vector<std::string> pinned = pinned_keys();
    const std::vector<oracle::row> rows = ledger::rows();
    REQUIRE_FALSE(pinned.empty());

    for(const std::string &key : pinned)
    {
        INFO("pinned entry point: " << key);
        CHECK(oracle::lookup(rows, key).has_value());
    }
    for(const oracle::row &one : rows)
    {
        REQUIRE(one.fields.size() == ledger::column_count);
        INFO("ledger row: " << one.fields[ledger::entry_point]);
        CHECK(std::ranges::find(pinned, one.fields[ledger::entry_point]) != pinned.end());
    }
    CHECK(rows.size() == pinned.size());
}

TEST_CASE("a ledger row's constructs are read against the closed vocabulary", "[ledger]")
{
    for(const oracle::row &one : ledger::rows())
    {
        INFO("ledger row: " << one.fields[ledger::entry_point]);
        ledger::set_of(ledger::spellings_in(one.fields[ledger::exercised_constructs]));
    }
    CHECK(ledger::set_of({ "alias", "unit-tag" }) == ledger::set_of({ "unit-tag", "alias" }));
    CHECK(meios::construct_from_name("unit_tag") == std::nullopt);
}

// The divergence column is an assertion in both directions, the way the divergence manifest
// beside this record is: an empty column claims the two results agree, and a filled one claims
// they do not, so a divergence recorded after it stopped reproducing fails here rather than
// quietly excusing a comparison.
TEST_CASE("a ledger row's divergence column is checked in both directions", "[ledger]")
{
    for(const oracle::row &one : ledger::rows())
    {
        INFO("ledger row: " << one.fields[ledger::entry_point]);
        const bool agree = one.fields[ledger::upstream_result] == one.fields[ledger::native_result];
        CHECK(agree == one.fields[ledger::reviewed_divergence].empty());
    }
}

// The pairing runs through the key and nothing else, which is what lets two entry points
// exercising the same constructs each keep their own row.
TEST_CASE("entry points exercising the same constructs still pair one to one", "[ledger]")
{
    const std::vector<oracle::row> rows = ledger::rows();
    REQUIRE_FALSE(rows.empty());
    std::vector<std::string> keys;
    for(const oracle::row &row : rows)
    {
        INFO("ledger row: " << row.fields[ledger::entry_point]);
        CHECK(std::ranges::find(keys, row.fields[ledger::entry_point]) == keys.end());
        keys.push_back(row.fields[ledger::entry_point]);
    }
}

TEST_CASE("each ledger row names the immutable revision the corpus fetches", "[ledger]")
{
    const std::string listfile = corpus::listfile_text("corpus.cmake");
    for(const oracle::row &one : ledger::rows())
    {
        const std::string &named = one.fields[ledger::revision];
        const std::size_t at = named.find('@');
        INFO("revision: " << named);
        REQUIRE(at != std::string::npos);
        CHECK(listfile.find("NAME " + named.substr(0, at)) != std::string::npos);
        CHECK(listfile.find("SHA256=" + named.substr(at + 1)) != std::string::npos);
    }
}

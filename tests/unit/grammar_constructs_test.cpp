#include "oracle_records.h"
#include "grammar_construct_pairs.h"
#include "grammar_construct_sites.h"
#include "grammar_construct_records.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstdint>
#include <iterator>
#include <algorithm>

namespace
{

using grammar::carrier;
using grammar::pairing;
using grammar::standing;

std::vector<pairing> all_pairings()
{
    std::vector<pairing> all(std::begin(grammar::element_pairings), std::end(grammar::element_pairings));
    all.insert(all.end(), std::begin(grammar::site_pairings), std::end(grammar::site_pairings));
    for(const grammar::unreached_site &one : grammar::unreached_sites)
        all.push_back(pairing{one.construct, carrier::unreachable, one.evidence, standing::open});
    return all;
}

std::string unpaired_constructs()
{
    const std::vector<pairing> all = all_pairings();
    std::string unpaired;
    for(const oracle::row &one : grammar::record("grammar_constructs.cases"))
    {
        const std::string &id = one.fields.front();
        const bool measured   = std::ranges::any_of(all, [&](const pairing &p) { return p.construct == id && p.by != carrier::boundary; });
        const bool bounded    = std::ranges::any_of(all, [&](const pairing &p) { return p.construct == id && p.by == carrier::boundary; });
        if(grammar::scope_of(id) == "grammar" ? measured : bounded)
            continue;
        unpaired += "\n  " + id;
    }
    return unpaired;
}

}

TEST_CASE("every pairing names a construct the record lists and a carrier its record holds", "[grammar_constructs]")
{
    for(const pairing &one : all_pairings())
    {
        INFO("pairing: " << one.construct << " / " << one.keys);
        const bool listed = oracle::lookup(grammar::record("grammar_constructs.cases"), one.construct).has_value();
        CHECK(listed);
        if(!listed)
            continue;
        CHECK(grammar::scope_held(one));
        const std::vector<std::string> keys = grammar::split_keys(one);
        CHECK_FALSE(keys.empty());
        for(const std::string &key : keys)
        {
            INFO("key: " << key);
            CHECK(grammar::key_held(one.by, key));
            CHECK(grammar::standing_held(one, key));
        }
    }
}

TEST_CASE("every construct the derived enumeration lists is paired, and by its scope", "[grammar_constructs]")
{
    for(const grammar::unreached_site &one : grammar::unreached_sites)
    {
        INFO("unreached site: " << one.construct);
        CHECK(std::ranges::count(all_pairings(), one.construct, &pairing::construct) == 1);
    }
    const std::string unpaired = unpaired_constructs();
    INFO("unpaired, in record order:" << unpaired);
    CHECK(unpaired.empty());
}

TEST_CASE("a deliberate pairing holds only where the manifest keeps its row", "[grammar_constructs]")
{
    const pairing kept{"tag.xacro_if", carrier::manifest, "conditional_real_one", standing::deliberate};
    CHECK(grammar::key_held(kept.by, kept.keys));
    CHECK(grammar::standing_held(kept, kept.keys));
    const pairing gone{"tag.xacro_if", carrier::manifest, "no_row_by_this_name", standing::deliberate};
    CHECK_FALSE(grammar::key_held(gone.by, gone.keys));
    CHECK_FALSE(grammar::standing_held(gone, gone.keys));
    for(const carrier by : {carrier::agreement, carrier::corpus, carrier::generated, carrier::expression, carrier::unit, carrier::boundary, carrier::unreachable})
    {
        INFO("carrier: " << static_cast<int32_t>(by));
        CHECK_FALSE(grammar::standing_held(pairing{kept.construct, by, kept.keys, standing::deliberate}, kept.keys));
    }
}

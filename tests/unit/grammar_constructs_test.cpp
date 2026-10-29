#include "oracle_records.h"
#include "grammar_construct_pairs.h"
#include "grammar_construct_sites.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <iterator>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <string_view>

namespace
{

using grammar::carrier;
using grammar::pairing;
using grammar::standing;

std::vector<std::string> split_keys(const pairing &one)
{
    if(one.by == carrier::unit || one.by == carrier::unreachable)
        return {std::string(one.keys)};
    std::istringstream words{std::string(one.keys)};
    return {std::istream_iterator<std::string>(words), std::istream_iterator<std::string>()};
}

std::vector<pairing> all_pairings()
{
    std::vector<pairing> all(std::begin(grammar::element_pairings), std::end(grammar::element_pairings));
    all.insert(all.end(), std::begin(grammar::site_pairings), std::end(grammar::site_pairings));
    for(const grammar::unreached_site &one : grammar::unreached_sites)
        all.push_back(pairing{one.construct, carrier::unreachable, one.evidence, standing::open});
    return all;
}

const std::vector<oracle::row> &record(std::string_view file)
{
    static std::map<std::string, std::vector<oracle::row>, std::less<>> loaded;
    auto found = loaded.find(file);
    if(found == loaded.end())
        found = loaded.emplace(std::string(file), oracle::load_rows(file)).first;
    return found->second;
}

std::optional<std::string> field_of(const std::vector<oracle::row> &rows, std::string_view key, std::size_t index)
{
    const std::optional<oracle::row> one = oracle::lookup(rows, key);
    if(!one || one->fields.size() <= index)
        return std::nullopt;
    return one->fields[index];
}

std::string file_text(const std::filesystem::path &file)
{
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::filesystem::path> stem_sources(std::string_view stem)
{
    std::istringstream lines{file_text(MEIOS_UNIT_STEM_SOURCES)};
    for(std::string line; std::getline(lines, line);)
    {
        std::istringstream fields{line};
        std::string name;
        std::getline(fields, name, '\t');
        std::vector<std::filesystem::path> sources;
        for(std::string one; name == stem && std::getline(fields, one, '\t');)
            sources.emplace_back(one);
        if(name == stem)
            return sources;
    }
    return {};
}

bool unit_case_held(std::string_view key)
{
    const std::size_t dot = key.find('.');
    if(dot == std::string_view::npos)
        return false;
    const std::string declared = "TEST_CASE(\"" + std::string(key.substr(dot + 1)) + '"';
    return std::ranges::any_of(stem_sources(key.substr(0, dot)), [&](const std::filesystem::path &source) { return file_text(source).find(declared) != std::string::npos; });
}

bool key_held(carrier by, std::string_view key)
{
    switch(by)
    {
        case carrier::agreement:
            return field_of(record("differential_inventory.cases"), key, 1) == "agreement";
        case carrier::corpus:
            return field_of(record("differential_inventory.cases"), key, 1) == "document";
        case carrier::manifest:
            return oracle::lookup(record("differential_divergences.cases"), key).has_value();
        case carrier::generated:
            return oracle::lookup(record("generated_cases.cases"), key).has_value();
        case carrier::expression:
            return oracle::lookup(record("expressions.cases"), key).has_value();
        case carrier::unit:
            return unit_case_held(key);
        case carrier::boundary:
            return key == "expression" || key == "tool";
        case carrier::unreachable:
            return !key.empty();
    }
    return false;
}

// What a recorded difference gives on this side, read from the row's third column; a key no
// difference row holds gives nothing.
std::optional<std::string> this_side(carrier by, std::string_view key)
{
    if(by == carrier::manifest || by == carrier::expression)
        return field_of(record("differential_divergences.cases"), key, 2);
    if(by == carrier::generated)
        return field_of(record("generated_divergences.cases"), key, 2);
    return std::nullopt;
}

bool standing_held(const pairing &one, std::string_view key)
{
    if(one.by == carrier::boundary || one.by == carrier::unreachable)
        return one.now == standing::open;
    const std::optional<std::string> here = this_side(one.by, key);
    if(one.now == standing::agrees)
        return one.by != carrier::manifest && !here;
    return here && here->starts_with("REFUSED") == (one.now == standing::refuses_listed);
}

std::string scope_of(std::string_view construct)
{
    return field_of(record("grammar_constructs.cases"), construct, 2).value_or("");
}

bool scope_held(const pairing &one)
{
    const std::string scope = scope_of(one.construct);
    if(one.by == carrier::boundary)
        return scope != "grammar" && scope == one.keys;
    return scope == "grammar";
}

std::string unpaired_constructs()
{
    const std::vector<pairing> all = all_pairings();
    std::string unpaired;
    for(const oracle::row &one : record("grammar_constructs.cases"))
    {
        const std::string &id = one.fields.front();
        const bool measured   = std::ranges::any_of(all, [&](const pairing &p) { return p.construct == id && p.by != carrier::boundary; });
        const bool bounded    = std::ranges::any_of(all, [&](const pairing &p) { return p.construct == id && p.by == carrier::boundary; });
        if(scope_of(id) == "grammar" ? measured : bounded)
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
        const bool listed = oracle::lookup(record("grammar_constructs.cases"), one.construct).has_value();
        CHECK(listed);
        if(!listed)
            continue;
        CHECK(scope_held(one));
        const std::vector<std::string> keys = split_keys(one);
        CHECK_FALSE(keys.empty());
        for(const std::string &key : keys)
        {
            INFO("key: " << key);
            CHECK(key_held(one.by, key));
            CHECK(standing_held(one, key));
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

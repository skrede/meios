#include "oracle_records.h"
#include "accepted_forms_coverage.h"

#include "meios/xacro/evaluator_constructs.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <sstream>
#include <cstddef>
#include <algorithm>
#include <string_view>

namespace
{

std::vector<std::string> split_keys(std::string_view spelling)
{
    std::istringstream words(std::string{ spelling });
    std::vector<std::string> keys;
    for(std::string one; words >> one;)
        keys.push_back(one);
    return keys;
}

std::vector<oracle::row> record_rows(std::string_view token)
{
    return oracle::load_rows(std::string(token) + ".cases");
}

// The records the enumeration points at, in the order it points at them and each named once.
std::vector<std::string> cited_records()
{
    std::vector<std::string> named;
    for(const accepted::coverage &one : accepted::form_coverage)
    {
        const std::string record(one.record);
        if(!record.empty() && std::ranges::find(named, record) == named.end())
            named.push_back(record);
    }
    return named;
}

std::vector<std::string> keys_cited_in(std::string_view record)
{
    std::vector<std::string> cited;
    for(const accepted::coverage &one : accepted::form_coverage)
    {
        if(one.record != record)
            continue;
        for(const std::string &key : split_keys(one.keys))
            if(std::ranges::find(cited, key) == cited.end())
                cited.push_back(key);
    }
    return cited;
}

std::vector<std::string> keys_present_in(const std::vector<oracle::row> &rows)
{
    std::vector<std::string> present;
    for(const oracle::row &one : rows)
    {
        REQUIRE_FALSE(one.fields.empty());
        if(std::ranges::find(present, one.fields.front()) == present.end())
            present.push_back(one.fields.front());
    }
    return present;
}

}

// Without the guard an emptied enumeration satisfies both loops below and the pairing reports
// total coverage of nothing, which is the failure this whole file exists to make impossible.
TEST_CASE("the enumeration and the records it points at are both non-empty", "[accepted_forms]")
{
    const std::vector<std::string> records = cited_records();
    REQUIRE_FALSE(records.empty());
    REQUIRE(accepted::form_count > 0);
    for(const std::string &record : records)
    {
        INFO("record: " << record);
        REQUIRE_FALSE(record_rows(record).empty());
        REQUIRE_FALSE(keys_cited_in(record).empty());
    }
}

TEST_CASE("every accepted form names a committed row or the reason it has none", "[accepted_forms]")
{
    for(const accepted::coverage &one : accepted::form_coverage)
    {
        INFO("accepted form: " << accepted::form_name(one.named));
        if(one.keys.empty())
        {
            CHECK_FALSE(one.reason.empty());
            continue;
        }
        const std::vector<oracle::row> rows = record_rows(one.record);
        for(const std::string &key : split_keys(one.keys))
        {
            INFO("key: " << key);
            CHECK(oracle::lookup(rows, key).has_value());
        }
    }
}

TEST_CASE("every committed row names an accepted form", "[accepted_forms]")
{
    for(const std::string &record : cited_records())
    {
        const std::vector<std::string> cited = keys_cited_in(record);
        for(const oracle::row &one : record_rows(record))
        {
            INFO("record " << record << ", row: " << one.fields.front());
            CHECK(std::ranges::find(cited, one.fields.front()) != cited.end());
        }
    }
}

// The count relation the two loops above cannot state between them: a form citing a key twice
// and a record carrying a row twice both leave the loops satisfied and the totals apart.
TEST_CASE("the enumeration cites exactly the keys the records carry", "[accepted_forms]")
{
    for(const std::string &record : cited_records())
    {
        INFO("record: " << record);
        CHECK(keys_cited_in(record).size() == keys_present_in(record_rows(record)).size());
    }
}

TEST_CASE("a spelling outside the table resolves to nothing", "[accepted_forms]")
{
    CHECK_FALSE(accepted::form_from_name("comprehension").has_value());
    CHECK_FALSE(accepted::form_from_name("").has_value());
    for(const accepted::form_spelling &one : accepted::form_spellings)
    {
        INFO("spelling: " << one.name);
        CHECK(accepted::form_from_name(one.name) == one.named);
    }
}

// The vocabulary a load reports is the one closed table in the library naming what an expression
// exercised, so a member added there without a form here fails. Its one coarser member is the
// unit tag, which this enumeration splits into the six tags the conversion table closes over.
TEST_CASE("every construct the load vocabulary names is an accepted form", "[accepted_forms]")
{
    for(const meios::construct_spelling &one : meios::construct_spellings)
    {
        INFO("construct: " << one.name);
        if(one.construct == meios::evaluator_construct::unit_tag)
        {
            CHECK(accepted::form_from_name("radians-tag").has_value());
            continue;
        }
        CHECK(accepted::form_from_name(one.name).has_value());
    }
}

// A row repeating another row's expression measures the same thing twice and makes the tier look
// broader than it is, so the duplicate is refused here rather than counted.
TEST_CASE("no two expression rows carry the same expression", "[accepted_forms]")
{
    std::vector<std::string> seen;
    for(const oracle::row &one : oracle::load_rows("expressions.cases"))
    {
        REQUIRE(one.fields.size() == 4);
        INFO("case " << one.fields[0] << ": " << one.fields[1]);
        CHECK(std::ranges::find(seen, one.fields[1]) == seen.end());
        seen.push_back(one.fields[1]);
    }
}

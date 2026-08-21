#include "oracle_records.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <string_view>

namespace
{

constexpr std::string_view measured[] = { "PINS",
                                          "collisions.cases",
                                          "compatibility_ledger.cases",
                                          "differential_divergences.cases",
                                          "differential_inventory.cases",
                                          "expressions.cases",
                                          "fer_facts.cases",
                                          "fp3_facts.cases",
                                          "fr3_facts.cases",
                                          "fr3v2_1_facts.cases",
                                          "fr3v2_facts.cases",
                                          "gantry_facts.cases",
                                          "gen3_facts.cases",
                                          "kr6_facts.cases",
                                          "lbr_med14_r820_facts.cases",
                                          "rendering.cases",
                                          "tmrv0_2_facts.cases",
                                          "unit_tags.cases",
                                          "ur3e_facts.cases",
                                          "ur5e_abs_paths_facts.cases",
                                          "ur5e_facts.cases",
                                          "ur5e_safety_facts.cases",
                                          "ur7e_facts.cases",
                                          "ur_mocked_facts.cases",
                                          "yaml_scalars.cases" };

std::string value_of(std::string_view file, std::string_view key)
{
    const std::optional<oracle::row> found = oracle::lookup(oracle::load_rows(file), key);
    REQUIRE(found.has_value());
    REQUIRE(found->fields.size() >= 2);
    return found->fields[1];
}

}

TEST_CASE("the upstream measurements are present, non-empty, and complete", "[oracle]")
{
    std::size_t total = 0;
    for(std::string_view file : measured)
    {
        INFO("record: " << file);
        const std::vector<oracle::row> rows = oracle::load_rows(file);
        REQUIRE_FALSE(rows.empty());
        total += rows.size();
    }
    REQUIRE(total == static_cast<std::size_t>(MEIOS_ORACLE_RECORD_ROWS));
}

TEST_CASE("the measurements name the upstream that produced them", "[oracle]")
{
    CHECK(value_of("PINS", "xacro") == "2.1.1");
    CHECK(value_of("PINS", "PyYAML") == "6.0.3");
    CHECK(value_of("PINS", "ur_description") == "4.3.1");
}

TEST_CASE("upstream renders small negative magnitudes in fixed notation", "[oracle]")
{
    CHECK(value_of("rendering.cases", "-0.0005") == "-0.0005");
    CHECK(value_of("rendering.cases", "-0.0001") == "-0.0001");
    CHECK(value_of("rendering.cases", "0.0004") == "0.0004");
}

TEST_CASE("upstream leaves a wrist joint without position limits its effort and velocity",
          "[oracle]")
{
    const std::string limit = value_of("ur3e_facts.cases", "joint.limit.wrist_3_joint");
    CHECK(value_of("ur3e_facts.cases", "joint.type.wrist_3_joint") == "continuous");
    CHECK(limit.find("lower=") == std::string::npos);
    CHECK(limit.find("upper=") == std::string::npos);
    CHECK(limit.find("effort=") != std::string::npos);
}

// The one case the refusal set turns on: a name the wrapper answers itself reads as a bound
// method by the attribute spelling and as the document's own value by the subscript spelling.
TEST_CASE("a colliding member name reads differently by each spelling, a plain key does not",
          "[oracle]")
{
    const std::vector<oracle::row> rows = oracle::load_rows("collisions.cases");

    std::size_t differing = 0;
    for(const oracle::row &one : rows)
    {
        INFO("name: " << one.fields.front());
        REQUIRE(one.fields.size() == 3);
        CHECK(one.fields.front().rfind("__", 0) != 0);
        if(one.fields[1] != one.fields[2])
            ++differing;
    }
    CHECK(differing + 1 == rows.size());
    CHECK(value_of("collisions.cases", "keys") == "method");
    CHECK(value_of("collisions.cases", "plain") != "method");
}

TEST_CASE("every measured row carries a subject and a measurement", "[oracle]")
{
    for(std::string_view file : measured)
    {
        INFO("record: " << file);
        const std::vector<oracle::row> rows = oracle::load_rows(file);
        REQUIRE_FALSE(rows.empty());
        for(const oracle::row &one : rows)
        {
            INFO("row: " << one.fields.front());
            CHECK(one.fields.size() >= 2);
        }
    }
}

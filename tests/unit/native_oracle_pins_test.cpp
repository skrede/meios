#include "oracle_records.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace
{

struct facts_source
{
    std::string_view record;
    std::string_view ledger_key;
};

constexpr facts_source sources[] = {
        {"ur5e_facts.cases", "ur_type=ur5e"},
        {"ur3e_facts.cases", "ur_type=ur3e"},
        {"ur7e_facts.cases", "ur_type=ur7e"},
        {"ur5e_safety_facts.cases", "safety_limits=true ur_type=ur5e"},
        {"ur5e_abs_paths_facts.cases", "force_abs_paths=true ur_type=ur5e"},
        {"kr6_facts.cases", "kr6r900sixx.xacro"},
        {"lbr_med14_r820_facts.cases", "lbr_med14_r820.urdf.xacro"},
        {"fer_facts.cases", "fer.urdf.xacro"},
        {"fp3_facts.cases", "fp3.urdf.xacro"},
        {"fr3_facts.cases", "fr3.urdf.xacro"},
        {"fr3v2_facts.cases", "fr3v2.urdf.xacro"},
        {"fr3v2_1_facts.cases", "fr3v2_1.urdf.xacro"},
        {"tmrv0_2_facts.cases", "tmrv0_2.urdf.xacro"},
        {"gen3_facts.cases", "dof=7"},
        {"ur_mocked_facts.cases", "name=ur_mocked ur_type=ur5e"},
        {"gantry_facts.cases", "gantry.urdf.xacro"},
};

std::vector<std::string> facts_on_disk()
{
    std::vector<std::string> found;
    for(const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(std::filesystem::path{MEIOS_GOLDEN_DIR} / "oracle"))
    {
        const std::string name = entry.path().filename().string();
        if(name.ends_with("_facts.cases"))
            found.push_back(name);
    }
    return found;
}

}

TEST_CASE("every facts record on disk has a known source", "[oracle]")
{
    const std::vector<std::string> found = facts_on_disk();
    REQUIRE(found.size() == std::size(sources));
    for(const std::string &name : found)
    {
        INFO("record: " << name);
        CHECK(std::any_of(std::begin(sources), std::end(sources), [&](const facts_source &one) { return one.record == name; }));
    }
}

// The ledger names the upstream and the archive digest each facts record was measured from; the
// pins must name that same upstream at that same digest, or a measurement stands on an unstated revision.
TEST_CASE("every facts record maps to a pinned source at the digest it was measured from", "[oracle]")
{
    const std::vector<oracle::row> ledger = oracle::load_rows("compatibility_ledger.cases");
    const std::vector<oracle::row> pins   = oracle::load_rows("PINS");
    for(const facts_source &one : sources)
    {
        INFO("record: " << one.record);
        const std::optional<oracle::row> entry = oracle::lookup(ledger, one.ledger_key);
        REQUIRE(entry.has_value());
        REQUIRE(entry->fields.size() >= 3);
        const std::string &revision = entry->fields[2];
        const std::size_t at        = revision.find('@');
        REQUIRE(at != std::string::npos);

        const std::optional<oracle::row> pin = oracle::lookup(pins, revision.substr(0, at));
        REQUIRE(pin.has_value());
        REQUIRE(pin->fields.size() == 3);
        CHECK_FALSE(pin->fields[1].empty());
        CHECK(pin->fields[2] == revision.substr(at + 1));
    }
}

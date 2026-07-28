#include "uri_table.h"
#include "uri_fixture.h"

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/claims.h>
#include <meios/diagnostic/completeness.h>
#include <meios/diagnostic/missing_asset.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

// to_string answers "unknown" for every value past the last enumerator, which is the only
// enumerator range the enum exposes.
std::set<std::string> code_spellings()
{
    std::set<std::string> out;
    for(int value = 0; value < 1024; ++value)
    {
        const std::string_view name = meios::to_string(static_cast<meios::diagnostic_code>(value));
        if(name == "unknown")
            break;
        out.insert(std::string{ name });
    }
    return out;
}

bool fixture_on_disk(const uri::row &r)
{
    return std::filesystem::exists(std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "uri"
                                   / r.fixture);
}

std::optional<uri::row> row_for_rule(std::string_view slug)
{
    for(const uri::row &r : uri::load_rows())
    {
        if(r.rule == slug)
            return r;
    }
    return std::nullopt;
}

std::vector<uri::row> rows_with_code(std::string_view code)
{
    std::vector<uri::row> out;
    for(const uri::row &r : uri::load_rows())
    {
        if(r.code == code)
            out.push_back(r);
    }
    return out;
}

}

TEST_CASE("uri table integrity", "[urdf][uri]")
{
    const std::vector<uri::row> rows = uri::load_rows();
    const std::set<std::string> spellings = code_spellings();
    REQUIRE_FALSE(rows.empty());
    REQUIRE_FALSE(spellings.empty());
    std::set<std::string> slugs;
    for(const uri::row &r : rows)
    {
        INFO(r.fixture);
        REQUIRE(uri::declared_verdict(r.verdict));
        REQUIRE(fixture_on_disk(r));
        REQUIRE(slugs.insert(r.rule).second);
        if(r.verdict == "accept")
            REQUIRE(r.code == uri::no_code);
        else
            REQUIRE(spellings.count(r.code) == 1);
    }
}

TEST_CASE("uri rows refused", "[urdf][uri]")
{
    for(const uri::row &r : uri::load_rows())
    {
        if(r.verdict != "refuse" || uri::texture_row(r))
            continue;
        INFO(r.fixture);
        CHECK(uri::located(uri::load_fixture(r.fixture).diagnostics, meios::level::error, r.code));
    }
}

TEST_CASE("uri rows accepted", "[urdf][uri]")
{
    for(const uri::row &r : uri::load_rows())
    {
        if(r.verdict != "accept" || uri::texture_row(r))
            continue;
        INFO(r.fixture);
        const uri::outcome result = uri::load_fixture(r.fixture);
        CHECK(result.succeeded);
        CHECK(uri::errors(result.diagnostics) == 0);
        CHECK(uri::resolved_mesh_path(result.robot).has_value());
    }
}

TEST_CASE("uri rows dropped", "[urdf][uri]")
{
    for(const uri::row &r : uri::load_rows())
    {
        if(r.verdict != "drop" || uri::texture_row(r))
            continue;
        INFO(r.fixture);
        const uri::outcome result = uri::load_fixture(r.fixture);
        CHECK(result.succeeded);
        CHECK(uri::located(result.diagnostics, meios::level::warn, r.code));
        const meios::completeness cleared = meios::cleared_by(uri::code_from(r.code));
        CHECK(meios::has(result.claims, meios::completeness::parsed)
              == !meios::has(cleared, meios::completeness::parsed));
    }
}

// Textures are resolved by a task of their own, so they get a case of their own: lumped in
// with the mesh rows they would leave that task without a signal it can read. The accepting
// arm stops at the absence of an error because material carries no resolved-texture field
// yet; the task that appends one strengthens this arm.
TEST_CASE("uri rows texture", "[urdf][uri]")
{
    for(const uri::row &r : uri::load_rows())
    {
        if(!uri::texture_row(r))
            continue;
        INFO(r.fixture);
        const uri::outcome result = uri::load_fixture(r.fixture);
        if(r.verdict == "accept")
        {
            CHECK(result.succeeded);
            CHECK(uri::errors(result.diagnostics) == 0);
            continue;
        }
        CHECK(uri::located(result.diagnostics, meios::level::error, r.code));
    }
}

// One of the three settings is the default, so an assertion driving only that one would pass
// while a containment refusal graded by policy violated the contract it is meant to pin.
TEST_CASE("uri rows unsoftened containment refusal", "[urdf][uri]")
{
    constexpr meios::missing_asset settings[] = { meios::missing_asset::skip,
                                                  meios::missing_asset::warn,
                                                  meios::missing_asset::fail };
    const std::vector<uri::row> uncontained = rows_with_code("uncontained_asset");
    REQUIRE_FALSE(uncontained.empty());
    for(const uri::row &r : uncontained)
    {
        for(meios::missing_asset setting : settings)
        {
            INFO(r.fixture);
            CHECK(uri::located(uri::load_fixture(r.fixture, setting).diagnostics,
                               meios::level::error, r.code));
        }
    }
}

// Containment that refuses everything is not a containment rule, and it would satisfy every
// refusal assertion in this file; this is the row that fails under one.
TEST_CASE("uri rows absolute inside a registered root", "[urdf][uri]")
{
    const std::optional<uri::row> inside = row_for_rule("absolute-path-inside-a-root");
    REQUIRE(inside);
    INFO(inside->fixture);
    const uri::outcome result = uri::load_fixture(inside->fixture);
    CHECK(result.succeeded);
    CHECK(uri::errors(result.diagnostics) == 0);
    CHECK(uri::resolved_mesh_path(result.robot).has_value());
}

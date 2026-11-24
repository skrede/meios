#include "uri_table.h"
#include "uri_fixture.h"

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/claims.h>
#include <meios/diagnostic/completeness.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>
#include <cstddef>
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
        out.insert(std::string{name});
    }
    return out;
}

bool fixture_on_disk(const uri::row &r)
{
    return std::filesystem::exists(std::filesystem::path{MEIOS_URDF_FIXTURE_DIR} / "uri" / r.fixture);
}

std::optional<meios::material<double>> first_material(const meios::model<double> &robot)
{
    for(const meios::link<double> &part : robot.links)
    {
        for(const meios::visual<double> &shown : part.visuals)
        {
            if(shown.material_inline)
                return shown.material_inline;
        }
    }
    return std::nullopt;
}

}

TEST_CASE("uri table integrity", "[urdf][uri]")
{
    const std::vector<uri::row> rows      = uri::load_rows();
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

// The table declares no dropping row today, because every URI decision the contract makes
// is a refusal or an acceptance. An empty loop would pass while asserting nothing, so the
// emptiness says so out loud and the case becomes live the moment a dropping row appears.
TEST_CASE("uri rows dropped", "[urdf][uri]")
{
    std::size_t driven = 0;
    for(const uri::row &r : uri::load_rows())
    {
        if(r.verdict != "drop" || uri::texture_row(r))
            continue;
        ++driven;
        INFO(r.fixture);
        const uri::outcome result = uri::load_fixture(r.fixture);
        CHECK(result.succeeded);
        CHECK(uri::located(result.diagnostics, meios::level::warn, r.code));
        const meios::completeness cleared = meios::cleared_by(uri::code_from(r.code));
        CHECK(meios::has(result.claims, meios::completeness::parsed) == !meios::has(cleared, meios::completeness::parsed));
    }
    if(driven == 0)
        SKIP("uri.cases declares no row whose verdict is drop; this case asserted nothing");
}

// Textures get a case of their own: lumped in with the mesh rows they would leave the
// resolution of a texture without a signal a reader can separate from a mesh's.
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
            const std::optional<meios::material<double>> shown = first_material(result.robot);
            REQUIRE(shown.has_value());
            REQUIRE(shown->texture.has_value());
            CHECK(shown->resolved_texture.has_value());
            CHECK(shown->resolved_texture != shown->texture);
            continue;
        }
        CHECK(uri::located(result.diagnostics, meios::level::error, r.code));
    }
}

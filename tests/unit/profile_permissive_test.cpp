#include "profile_table.h"
#include "profile_fixture.h"

#include <meios/urdf/policy.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/claims.h>
#include <meios/diagnostic/completeness.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>

namespace
{

constexpr meios::strictness permissive[] = { meios::strictness::warn, meios::strictness::skip };

meios::load_options at(meios::strictness strict)
{
    meios::load_options opts;
    opts.strict = strict;
    return opts;
}

bool parsed(const profile::outcome &result)
{
    return meios::has(result.claims, meios::completeness::parsed);
}

bool clears_the_parse_claim(const std::string &code)
{
    return meios::has(meios::cleared_by(profile::code_from(code)), meios::completeness::parsed);
}

}

TEST_CASE("a scale the reader refuses drops the shape rather than scaling a mesh to nothing",
          "[urdf][profile][permissive]")
{
    REQUIRE_FALSE(profile::load_fixture("mesh_scale_short.urdf").succeeded);
    for(meios::strictness strict : permissive)
    {
        const profile::outcome result = profile::load_fixture("mesh_scale_short.urdf", at(strict));
        REQUIRE(result.succeeded);
        REQUIRE(result.robot.links.size() == 1);
        REQUIRE(result.robot.links.front().visuals.empty());
    }
}

TEST_CASE("a color the reader refuses drops the material rather than inventing opaque black",
          "[urdf][profile][permissive]")
{
    REQUIRE_FALSE(profile::load_fixture("color_rgba_short.urdf").succeeded);
    for(meios::strictness strict : permissive)
    {
        const profile::outcome result = profile::load_fixture("color_rgba_short.urdf", at(strict));
        REQUIRE(result.succeeded);
        REQUIRE(result.robot.materials.empty());
    }
}

TEST_CASE("a visual whose origin the reader refuses is dropped whole", "[urdf][profile][permissive]")
{
    REQUIRE_FALSE(profile::load_fixture("visual_origin_short.urdf").succeeded);
    for(meios::strictness strict : permissive)
    {
        const profile::outcome result =
            profile::load_fixture("visual_origin_short.urdf", at(strict));
        REQUIRE(result.succeeded);
        REQUIRE(result.robot.links.size() == 1);
        REQUIRE(result.robot.links.front().visuals.empty());
    }
}

// The two permissive settings differ in what a consumer is told and in nothing else, which
// is the property that keeps the removed coercions out of reach of any policy value.
TEST_CASE("the setting grades the diagnostic and never the enforcement",
          "[urdf][profile][permissive]")
{
    const profile::outcome spoken =
        profile::load_fixture("mesh_scale_short.urdf", at(meios::strictness::warn));
    REQUIRE(profile::located(spoken.diagnostics, meios::level::warn, "vector_arity"));

    const profile::outcome silent =
        profile::load_fixture("mesh_scale_short.urdf", at(meios::strictness::skip));
    REQUIRE(silent.diagnostics.empty());
    REQUIRE(silent.robot.links.front().visuals.empty());
}

TEST_CASE("a drop withdraws the parse claim even where nothing is said about it",
          "[urdf][profile][permissive][claims]")
{
    const std::string dropping[] = { "mesh_scale_short.urdf", "color_rgba_short.urdf",
                                     "visual_origin_short.urdf", "inertial_mass_absent.urdf" };
    for(const std::string &fixture : dropping)
    {
        INFO(fixture);
        const profile::outcome result = profile::load_fixture(fixture, at(meios::strictness::skip));
        REQUIRE(result.succeeded);
        REQUIRE(result.diagnostics.empty());
        REQUIRE_FALSE(parsed(result));
    }
}

TEST_CASE("an inertial the reader refuses leaves no body behind at the silent setting",
          "[urdf][profile][permissive]")
{
    const profile::outcome result =
        profile::load_fixture("inertial_mass_absent.urdf", at(meios::strictness::skip));
    REQUIRE(result.succeeded);
    REQUIRE(result.robot.links.size() == 1);
    REQUIRE_FALSE(result.robot.links.front().body.has_value());
}

TEST_CASE("a document carries the same claims whether or not it is spoken about",
          "[urdf][profile][permissive][claims]")
{
    std::size_t exercised = 0;
    for(const profile::row &r : profile::load_rows())
    {
        INFO(r.fixture);
        const profile::outcome spoken =
            profile::load_fixture(r.fixture, at(meios::strictness::warn));
        const profile::outcome silent =
            profile::load_fixture(r.fixture, at(meios::strictness::skip));
        REQUIRE(spoken.succeeded == silent.succeeded);
        if(!spoken.succeeded)
            continue;
        ++exercised;
        REQUIRE(spoken.claims == silent.claims);
    }
    REQUIRE(exercised >= 10);
}

// The claim answers for the document rather than for the log, so a row that still loads
// once its diagnostic is silenced must answer exactly as it does when the diagnostic is
// spoken. Nothing asserted a claim at this setting before, which is why it could drift.
TEST_CASE("every row that survives the silent setting still answers for its parse claim",
          "[urdf][profile][permissive][claims]")
{
    std::size_t exercised = 0;
    for(const profile::row &r : profile::load_rows())
    {
        const profile::outcome result = profile::load_fixture(r.fixture, at(meios::strictness::skip));
        if(!result.succeeded)
            continue;
        INFO(r.fixture);
        ++exercised;
        if(r.verdict == "accept")
            REQUIRE(parsed(result));
        else
            REQUIRE(parsed(result) == !clears_the_parse_claim(r.code));
    }
    REQUIRE(exercised >= 10);
}

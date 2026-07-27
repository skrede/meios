#include "profile_fixture.h"

#include <meios/urdf/policy.h>

#include <meios/diagnostic/level.h>

#include <catch2/catch_test_macros.hpp>

namespace
{

constexpr meios::strictness permissive[] = { meios::strictness::warn, meios::strictness::skip };

meios::load_options at(meios::strictness strict)
{
    meios::load_options opts;
    opts.strict = strict;
    return opts;
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

TEST_CASE("an inertial the reader refuses leaves no body behind at the silent setting",
          "[urdf][profile][permissive]")
{
    const profile::outcome result =
        profile::load_fixture("inertial_mass_absent.urdf", at(meios::strictness::skip));
    REQUIRE(result.succeeded);
    REQUIRE(result.robot.links.size() == 1);
    REQUIRE_FALSE(result.robot.links.front().body.has_value());
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

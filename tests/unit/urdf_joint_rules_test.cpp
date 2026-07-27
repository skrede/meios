#include "field_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>

using probe::carries;
using probe::fixture;
using probe::outcome;
using probe::walk;

namespace
{

meios::expected<meios::load_result, meios::load_error> load_profile(const std::string &name)
{
    return meios::load(fixture(name));
}

}

TEST_CASE("an absent joint type and an unrecognized one are two different refusals",
          "[urdf][joint-rules]")
{
    const meios::expected<meios::load_result, meios::load_error> absent =
        load_profile("joint_type_absent.urdf");
    const meios::expected<meios::load_result, meios::load_error> unknown =
        load_profile("joint_type_unknown.urdf");

    REQUIRE_FALSE(absent);
    REQUIRE_FALSE(unknown);
    REQUIRE(absent.error().code == meios::diagnostic_code::missing_joint_type);
    REQUIRE(unknown.error().code == meios::diagnostic_code::unknown_joint_type);
    REQUIRE(absent.error().code != unknown.error().code);

    const std::string named = unknown.error().message;
    std::size_t spelled = 0;
    for(const char *kind : { "revolute", "continuous", "prismatic", "fixed", "floating", "planar" })
        if(named.find(kind) != std::string::npos)
            ++spelled;
    REQUIRE(spelled >= 3);
}

TEST_CASE("neither joint-type failure reaches a sink as a rigid connection", "[urdf][joint-rules]")
{
    const std::string body = "  <link name=\"base\"/>\n  <link name=\"tip\"/>\n"
                             "  <joint name=\"j\">\n    <parent link=\"base\"/>\n"
                             "    <child link=\"tip\"/>\n  </joint>\n";
    for(const meios::strictness setting :
        { meios::strictness::fail, meios::strictness::warn, meios::strictness::skip })
    {
        const outcome out = walk(body, setting);
        REQUIRE(carries(out, meios::level::error, meios::diagnostic_code::missing_joint_type));
        REQUIRE(out.robot.joints.empty());
    }
}

TEST_CASE("a bounded joint must declare a limit, in the library rather than in a verb",
          "[urdf][joint-rules]")
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_profile("joint_limit_absent.urdf");

    REQUIRE_FALSE(loaded);
    REQUIRE(loaded.error().code == meios::diagnostic_code::missing_limit);
    REQUIRE(loaded.error().loc.line > 0);
}

TEST_CASE("the defaults the wiki states survive the enforcement", "[urdf][joint-rules]")
{
    const meios::expected<meios::load_result, meios::load_error> plain =
        load_profile("wiki_defaults.urdf");
    REQUIRE(plain);
    const meios::joint<double> &edge = plain->robot.joints.at(0);
    REQUIRE(edge.origin.translation.x == 0.0);
    REQUIRE(edge.origin.translation.z == 0.0);
    REQUIRE(edge.axis.x == 1.0);
    REQUIRE(edge.axis.y == 0.0);

    const meios::expected<meios::load_result, meios::load_error> mimicking =
        load_profile("mimic_multiplier_default.urdf");
    REQUIRE(mimicking);
    const meios::mimic &couple = *mimicking->robot.joints.at(1).couple;
    REQUIRE(couple.multiplier == 1.0);
    REQUIRE(couple.offset == 0.0);
}

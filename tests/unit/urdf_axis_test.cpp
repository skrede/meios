#include "field_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using probe::carries;
using probe::errors;
using probe::fixture;
using probe::outcome;
using probe::walk;

namespace
{

std::string joint_of(const std::string &kind, const std::string &axis)
{
    return "  <link name=\"base\"/>\n  <link name=\"tip\"/>\n  <joint name=\"j\" type=\"" + kind
        + "\">\n    <parent link=\"base\"/>\n    <child link=\"tip\"/>\n" + axis
        + "    <limit effort=\"1\" velocity=\"1\" lower=\"0\" upper=\"1\"/>\n  </joint>\n";
}

meios::expected<meios::load_result, meios::load_error> load_profile(const std::string &name)
{
    return meios::load(fixture(name));
}

}

TEST_CASE("every joint kind that uses its axis refuses a zero one", "[urdf][axis]")
{
    for(const char *kind : { "revolute", "continuous", "prismatic", "planar" })
    {
        INFO(kind);
        const outcome out = walk(joint_of(kind, "    <axis xyz=\"0 0 0\"/>\n"),
                                 meios::strictness::fail);
        REQUIRE(carries(out, meios::level::error, meios::diagnostic_code::zero_axis));
        REQUIRE(out.robot.joints.empty());
    }
}

TEST_CASE("the two kinds the wiki excludes from the axis field accept a zero axis",
          "[urdf][axis]")
{
    for(const char *kind : { "fixed", "floating" })
    {
        INFO(kind);
        const outcome out = walk(joint_of(kind, "    <axis xyz=\"0 0 0\"/>\n"),
                                 meios::strictness::fail);
        REQUIRE(errors(out) == 0);
        REQUIRE(out.robot.joints.size() == 1);
    }
}

TEST_CASE("a zero axis on a fixed joint loads clean through the whole entry point",
          "[urdf][axis]")
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_profile("zero_axis_fixed.urdf");

    REQUIRE(loaded);
    REQUIRE(loaded->robot.joints.size() == 1);
}

TEST_CASE("a zero axis on a turning joint is refused at a real location", "[urdf][axis]")
{
    for(const char *name : { "zero_axis_revolute.urdf", "zero_axis_prismatic.urdf" })
    {
        INFO(name);
        const meios::expected<meios::load_result, meios::load_error> loaded = load_profile(name);
        REQUIRE_FALSE(loaded);
        REQUIRE(loaded.error().code == meios::diagnostic_code::zero_axis);
        REQUIRE_FALSE(loaded.error().loc.file.empty());
        REQUIRE(loaded.error().loc.line > 0);
    }
}

TEST_CASE("a non-unit axis reaches the record with the components the author wrote",
          "[urdf][axis]")
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_profile("non_unit_axis.urdf");

    REQUIRE(loaded);
    const meios::vector3<double> &axis = loaded->robot.joints.at(0).axis;
    REQUIRE(axis.x == 0.0);
    REQUIRE(axis.y == 0.0);
    REQUIRE(axis.z == 2.0);
}

TEST_CASE("an axis element carrying no coordinates is refused rather than left at zero",
          "[urdf][axis]")
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_profile("axis_no_xyz.urdf");

    REQUIRE_FALSE(loaded);
    REQUIRE(loaded.error().code == meios::diagnostic_code::missing_required_field);
    REQUIRE(loaded.error().loc.line > 0);
}

TEST_CASE("a joint declaring no axis element at all keeps the documented default",
          "[urdf][axis]")
{
    const outcome out = walk(joint_of("revolute", ""), meios::strictness::fail);

    REQUIRE(errors(out) == 0);
    REQUIRE(out.robot.joints.at(0).axis.x == 1.0);
    REQUIRE(out.robot.joints.at(0).axis.y == 0.0);
    REQUIRE(out.robot.joints.at(0).axis.z == 0.0);
}

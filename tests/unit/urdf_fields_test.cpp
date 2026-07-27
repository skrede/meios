#include "field_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using probe::carries;
using probe::errors;
using probe::mentions;
using probe::outcome;
using probe::walk;

namespace
{

const std::string inertial_without_mass =
    "  <link name=\"base\">\n"
    "    <inertial><inertia ixx=\"1\" ixy=\"0\" ixz=\"0\" iyy=\"1\" iyz=\"0\" izz=\"1\"/></inertial>\n"
    "  </link>\n";

const std::string visual_with_unknown_shape =
    "  <link name=\"base\">\n"
    "    <visual><geometry><trapezoid/></geometry></visual>\n"
    "  </link>\n";

std::string one_joint(const std::string &attributes, const std::string &children)
{
    return "  <link name=\"base\"/>\n  <link name=\"tip\"/>\n  <joint name=\"j\" " + attributes
         + ">\n    <parent link=\"base\"/>\n    <child link=\"tip\"/>\n" + children + "  </joint>\n";
}

}

TEST_CASE("a required part that is missing drops its containing element at every setting",
          "[urdf][fields]")
{
    const meios::diagnostic_code code = meios::diagnostic_code::missing_required_field;

    const outcome refused = walk(inertial_without_mass, meios::strictness::fail);
    REQUIRE(carries(refused, meios::level::error, code));

    const outcome warned = walk(inertial_without_mass, meios::strictness::warn);
    REQUIRE(carries(warned, meios::level::warn, code));
    REQUIRE(errors(warned) == 0);

    const outcome quiet = walk(inertial_without_mass, meios::strictness::skip);
    REQUIRE_FALSE(mentions(quiet, code));

    for(const outcome &out : { refused, warned, quiet })
    {
        REQUIRE(out.robot.links.size() == 1);
        REQUIRE_FALSE(out.robot.links.at(0).body.has_value());
    }
}

TEST_CASE("a malformed shape drops its visual instead of becoming an empty mesh", "[urdf][fields]")
{
    const meios::diagnostic_code code = meios::diagnostic_code::unknown_geometry_shape;

    const outcome refused = walk(visual_with_unknown_shape, meios::strictness::fail);
    REQUIRE(carries(refused, meios::level::error, code));
    REQUIRE(probe::message_for(refused, code).find("trapezoid") != std::string::npos);

    const outcome warned = walk(visual_with_unknown_shape, meios::strictness::warn);
    REQUIRE(carries(warned, meios::level::warn, code));
    REQUIRE(errors(warned) == 0);

    const outcome quiet = walk(visual_with_unknown_shape, meios::strictness::skip);
    REQUIRE_FALSE(mentions(quiet, code));

    for(const outcome &out : { refused, warned, quiet })
        REQUIRE(out.robot.links.at(0).visuals.empty());
}

TEST_CASE("the calibration reader tells an absent attribute from an unreadable one",
          "[urdf][fields]")
{
    const outcome silent =
        walk(one_joint("type=\"continuous\"", "    <calibration/>\n"), meios::strictness::fail);
    REQUIRE(silent.notes.empty());
    REQUIRE(silent.robot.joints.at(0).calib.has_value());
    REQUIRE_FALSE(silent.robot.joints.at(0).calib->rising.has_value());

    const outcome loud = walk(one_joint("type=\"continuous\"", "    <calibration rising=\"soon\"/>\n"),
                              meios::strictness::fail);
    REQUIRE(carries(loud, meios::level::error, meios::diagnostic_code::invalid_number));
    REQUIRE_FALSE(loud.robot.joints.at(0).calib->rising.has_value());
}

TEST_CASE("a non-finite value is refused under a code rather than under none", "[urdf][fields]")
{
    const outcome out =
        walk(one_joint("type=\"fixed\"", "    <origin xyz=\"inf 0 0\"/>\n"), meios::strictness::fail);

    REQUIRE(carries(out, meios::level::error, meios::diagnostic_code::invalid_number));
    REQUIRE_FALSE(mentions(out, meios::diagnostic_code::unspecified));
}

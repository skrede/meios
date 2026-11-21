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

std::string offset_joint(const std::string &xyz)
{
    return one_joint("type=\"fixed\"", "    <origin xyz=\"" + xyz + "\"/>\n");
}

std::string massive_link(const std::string &mass)
{
    return "  <link name=\"base\">\n    <inertial><mass value=\"" + mass + "\"/>"
           "<inertia ixx=\"1\" ixy=\"0\" ixz=\"0\" iyy=\"1\" iyz=\"0\" izz=\"1\"/></inertial>\n"
           "  </link>\n";
}

struct offset_reading
{
    std::string            xyz;
    meios::vector3<double> expected;
};

struct mass_reading
{
    std::string mass;
    double      expected;
};

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

// The guard the Universal Robots macro writes for a continuous joint names two of the four, and
// the record names exactly what the element declared. The reader must keep such a guard rather
// than drop it for incompleteness, or the record's row and the load disagree on its presence.
TEST_CASE("a guard declaring some of its attributes is kept rather than dropped", "[urdf][fields]")
{
    const outcome out = walk(one_joint("type=\"continuous\"",
                                       "    <safety_controller k_position=\"20\" "
                                       "k_velocity=\"0.0\"/>\n"),
                             meios::strictness::fail);

    REQUIRE(out.notes.empty());
    REQUIRE(out.robot.joints.at(0).safety.has_value());
    CHECK(out.robot.joints.at(0).safety->k_position == 20.0);
    CHECK(out.robot.joints.at(0).safety->k_velocity == 0.0);
}

TEST_CASE("a non-finite value is refused under a code rather than under none", "[urdf][fields]")
{
    const outcome out =
        walk(one_joint("type=\"fixed\"", "    <origin xyz=\"inf 0 0\"/>\n"), meios::strictness::fail);

    REQUIRE(carries(out, meios::level::error, meios::diagnostic_code::invalid_number));
    REQUIRE_FALSE(mentions(out, meios::diagnostic_code::unspecified));
}

TEST_CASE("a number after leading white space or one plus sign is read as urdfdom reads it",
          "[urdf][fields]")
{
    for(const offset_reading &one : { offset_reading{ "+1 0 0", { 1.0, 0.0, 0.0 } },
                                      offset_reading{ "+1 +2.5 -3", { 1.0, 2.5, -3.0 } } })
    {
        INFO("xyz '" << one.xyz << "'");
        const outcome out = walk(offset_joint(one.xyz), meios::strictness::fail);
        CHECK(out.notes.empty());
        const meios::vector3<double> &at = out.robot.joints.at(0).origin.translation;
        CHECK((at.x == one.expected.x && at.y == one.expected.y && at.z == one.expected.z));
    }
    for(const mass_reading &one : { mass_reading{ " 1.0", 1.0 }, mass_reading{ "+0.5e1", 5.0 },
                                    mass_reading{ "\t\n +2", 2.0 } })
    {
        INFO("mass '" << one.mass << "'");
        const outcome out = walk(massive_link(one.mass), meios::strictness::fail);
        CHECK(out.notes.empty());
        const meios::link<double> &base = out.robot.links.at(0);
        CHECK((base.body.has_value() && base.body->mass == one.expected));
    }
}

TEST_CASE("a doubled sign, a lone plus, trailing white space or a blank still refuses the number",
          "[urdf][fields]")
{
    for(const std::string refused : { "++1", "+-1", "-+1", "+", "+ 1", "1.0 ", " ", "", "+inf" })
    {
        INFO("mass '" << refused << "'");
        const outcome out = walk(massive_link(refused), meios::strictness::fail);
        CHECK(carries(out, meios::level::error, meios::diagnostic_code::invalid_number));
    }
    const outcome out = walk(offset_joint("++1 0 0"), meios::strictness::fail);
    CHECK(carries(out, meios::level::error, meios::diagnostic_code::invalid_number));
}

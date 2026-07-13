#include <meios/records/robot_info.h>
#include <meios/records/material.h>
#include <meios/records/geometry.h>
#include <meios/records/inertial.h>
#include <meios/records/extension.h>
#include <meios/records/visual.h>
#include <meios/records/collision.h>
#include <meios/records/link.h>
#include <meios/records/joint.h>
#include <meios/records/joint_extras.h>
#include <meios/records/loop_constraint.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>
#include <optional>
#include <type_traits>

static_assert(std::is_aggregate_v<meios::extension>);
static_assert(std::is_aggregate_v<meios::robot_info>);
static_assert(std::is_aggregate_v<meios::material<double>>);
static_assert(std::is_aggregate_v<meios::material<float>>);
static_assert(std::is_aggregate_v<meios::geometry<double>>);
static_assert(std::is_aggregate_v<meios::geometry<float>>);
static_assert(std::is_aggregate_v<meios::inertial<double>>);
static_assert(std::is_aggregate_v<meios::inertial<float>>);
static_assert(std::is_aggregate_v<meios::visual<double>>);
static_assert(std::is_aggregate_v<meios::visual<float>>);
static_assert(std::is_aggregate_v<meios::collision<double>>);
static_assert(std::is_aggregate_v<meios::collision<float>>);
static_assert(std::is_aggregate_v<meios::link<double>>);
static_assert(std::is_aggregate_v<meios::link<float>>);
static_assert(std::is_aggregate_v<meios::joint<double>>);
static_assert(std::is_aggregate_v<meios::joint<float>>);
static_assert(std::is_aggregate_v<meios::joint_limits<double>>);
static_assert(std::is_aggregate_v<meios::mimic>);
static_assert(std::is_aggregate_v<meios::dynamics<double>>);
static_assert(std::is_aggregate_v<meios::safety_controller<double>>);
static_assert(std::is_aggregate_v<meios::calibration<double>>);
static_assert(std::is_aggregate_v<meios::loop_constraint<double>>);

static_assert(
    std::is_same_v<decltype(meios::inertial<double>{}.origin), meios::transform<double>>);
static_assert(
    std::is_same_v<decltype(meios::inertial<double>{}.tensor), meios::inertia<double>>);

TEST_CASE("geometry value-inits to a default-constructible first alternative", "[model][records]")
{
    const meios::geometry<double> geom{};
    REQUIRE(geom.shape.index() == 0);
    REQUIRE(std::holds_alternative<meios::mesh<double>>(geom.shape));
}

TEST_CASE("a geometry variant is visitable across all four shapes", "[model][records]")
{
    const meios::geometry<double> shapes[] = {
        { .shape = meios::mesh<double>{ .filename = "arm.stl", .scale = { .x = 1.0, .y = 1.0, .z = 1.0 } } },
        { .shape = meios::box<double>{ .size = { .x = 1.0, .y = 2.0, .z = 3.0 } } },
        { .shape = meios::cylinder<double>{ .radius = 0.5, .length = 2.0 } },
        { .shape = meios::sphere<double>{ .radius = 0.25 } },
    };

    int visited = 0;
    for (const auto& geom : shapes)
    {
        std::visit([&visited](const auto&) { ++visited; }, geom.shape);
    }
    REQUIRE(visited == 4);
    REQUIRE(std::holds_alternative<meios::mesh<double>>(shapes[0].shape));
    REQUIRE(std::holds_alternative<meios::box<double>>(shapes[1].shape));
    REQUIRE(std::holds_alternative<meios::cylinder<double>>(shapes[2].shape));
    REQUIRE(std::holds_alternative<meios::sphere<double>>(shapes[3].shape));
}

TEST_CASE("a link aggregate-inits with one visual and one collision", "[model][records]")
{
    const meios::link<double> arm{
        .name = "arm",
        .visuals = { { .origin = {},
                       .geom = { .shape = meios::box<double>{ .size = { .x = 1.0, .y = 2.0, .z = 3.0 } } },
                       .material_ref = std::string{ "steel" } } },
        .collisions = { { .origin = {},
                          .geom = { .shape = meios::box<double>{ .size = { .x = 1.0, .y = 2.0, .z = 3.0 } } } } },
    };

    REQUIRE(arm.name == "arm");
    REQUIRE_FALSE(arm.body.has_value());
    REQUIRE(arm.visuals.size() == 1);
    REQUIRE(arm.collisions.size() == 1);
    REQUIRE(arm.extensions.empty());
    REQUIRE(arm.visuals.front().material_ref == "steel");
    REQUIRE(std::holds_alternative<meios::box<double>>(arm.visuals.front().geom.shape));
}

TEST_CASE("all six joint kinds are distinct with absent optionals by default", "[model][records]")
{
    const meios::joint_kind kinds[] = {
        meios::joint_kind::fixed,      meios::joint_kind::revolute,
        meios::joint_kind::continuous, meios::joint_kind::prismatic,
        meios::joint_kind::floating,   meios::joint_kind::planar,
    };

    for (std::size_t i = 0; i < std::size(kinds); ++i)
    {
        for (std::size_t j = i + 1; j < std::size(kinds); ++j)
        {
            REQUIRE(kinds[i] != kinds[j]);
        }
    }

    const meios::joint<double> j{ .name = "shoulder",
                                  .kind = meios::joint_kind::revolute,
                                  .parent = "base",
                                  .child = "arm",
                                  .origin = {},
                                  .axis = { .x = 0.0, .y = 0.0, .z = 1.0 } };
    REQUIRE(j.parent == "base");
    REQUIRE(j.child == "arm");
    REQUIRE_FALSE(j.limits.has_value());
    REQUIRE_FALSE(j.couple.has_value());
    REQUIRE_FALSE(j.dyn.has_value());
    REQUIRE_FALSE(j.safety.has_value());
    REQUIRE_FALSE(j.calib.has_value());
    REQUIRE_FALSE(j.closes_loop);
    REQUIRE(j.extensions.empty());
}

TEST_CASE("robot_info, link and joint start with an empty extensions slot", "[model][records]")
{
    const meios::robot_info info{};
    const meios::link<double> lk{};
    const meios::joint<double> jt{};
    REQUIRE(info.extensions.empty());
    REQUIRE(lk.extensions.empty());
    REQUIRE(jt.extensions.empty());
}

TEST_CASE("a pushed extension captures element name and attributes as data", "[model][records]")
{
    meios::link<double> sensor_link{ .name = "camera_link" };
    sensor_link.extensions.push_back(
        meios::extension{ .element = "gazebo",
                          .content = "",
                          .attributes = { { "reference", "camera_link" } } });

    REQUIRE(sensor_link.extensions.size() == 1);
    REQUIRE(sensor_link.extensions.front().element == "gazebo");
    REQUIRE(sensor_link.extensions.front().attributes.size() == 1);
    REQUIRE(sensor_link.extensions.front().attributes.front().first == "reference");
    REQUIRE(sensor_link.extensions.front().attributes.front().second == "camera_link");
}

TEST_CASE("an inertial carries an un-baked transform origin and a named tensor", "[model][records]")
{
    const meios::inertial<double> body{
        .origin = { .translation = { .x = 0.0, .y = 0.0, .z = 0.1 }, .rotation = {} },
        .mass = 2.0,
        .tensor = { .ixx = 1.0, .ixy = 0.0, .ixz = 0.0, .iyy = 2.0, .iyz = 0.0, .izz = 3.0 },
    };
    REQUIRE(body.origin.translation.z == 0.1);
    REQUIRE(body.mass == 2.0);
    REQUIRE(body.tensor.izz == 3.0);
}

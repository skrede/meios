#include <meios/math/inertia.h>
#include <meios/math/vector3.h>
#include <meios/math/rotations.h>
#include <meios/math/transform.h>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

static_assert(std::is_aggregate_v<meios::vector3<double>>);
static_assert(std::is_aggregate_v<meios::vector3<float>>);
static_assert(std::is_aggregate_v<meios::rpy<double>>);
static_assert(std::is_aggregate_v<meios::rpy<float>>);
static_assert(std::is_aggregate_v<meios::quaternion<double>>);
static_assert(std::is_aggregate_v<meios::quaternion<float>>);
static_assert(std::is_aggregate_v<meios::axis_angle<double>>);
static_assert(std::is_aggregate_v<meios::axis_angle<float>>);
static_assert(std::is_aggregate_v<meios::inertia<double>>);
static_assert(std::is_aggregate_v<meios::inertia<float>>);
static_assert(std::is_aggregate_v<meios::transform<double>>);
static_assert(std::is_aggregate_v<meios::transform<float>>);

static_assert(
    std::is_same_v<decltype(meios::transform<double>{}.rotation), meios::rpy<double>>);
static_assert(
    std::is_same_v<decltype(meios::transform<double, meios::quaternion<double>>{}.rotation),
                   meios::quaternion<double>>);

TEST_CASE("transform value-inits to a zero identity origin", "[model][math]")
{
    const meios::transform<double> origin{};
    REQUIRE(origin.translation.x == 0.0);
    REQUIRE(origin.translation.y == 0.0);
    REQUIRE(origin.translation.z == 0.0);
    REQUIRE(origin.rotation.roll == 0.0);
    REQUIRE(origin.rotation.pitch == 0.0);
    REQUIRE(origin.rotation.yaw == 0.0);
}

TEST_CASE("designated initializers read back in declaration order", "[model][math]")
{
    const meios::vector3<double> v{ .x = 1.0, .y = 2.0, .z = 3.0 };
    REQUIRE(v.x == 1.0);
    REQUIRE(v.y == 2.0);
    REQUIRE(v.z == 3.0);

    const meios::inertia<double> tensor{ .ixx = 1.0, .ixy = 0.0, .ixz = 0.0,
                                         .iyy = 2.0, .iyz = 0.0, .izz = 3.0 };
    REQUIRE(tensor.ixx == 1.0);
    REQUIRE(tensor.iyy == 2.0);
    REQUIRE(tensor.izz == 3.0);
}

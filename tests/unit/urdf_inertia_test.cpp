#include "field_probe.h"

#include "urdf_detail.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <limits>

using probe::fixture;

namespace
{

meios::expected<meios::load_result, meios::load_error> load_profile(const std::string &name)
{
    return meios::load(fixture(name));
}

// The checker is a compiled core-private predicate, so a case that no document can reach —
// a non-finite component, which the numeric reader refuses first — is driven directly.
struct checker
{
    meios::source_stack   sources;
    meios::core_evaluator eval;
    meios::log_sink       log;
    meios::parse_context  ctx;

    checker()
        : sources{}, eval{}, log{},
          ctx{ sources,
               eval,
               log,
               meios::missing_asset::warn,
               meios::topology_policy::fail,
               meios::material_policy::warn,
               meios::strictness::skip,
               "inertia.urdf" }
    {
    }

    bool accepts(double mass, const meios::inertia<double> &tensor)
    {
        meios::inertial<double> body{};
        body.mass = mass;
        body.tensor = tensor;
        return meios::detail::check_inertia(body, ctx, meios::source_location{ {}, 1, 1 });
    }
};

// A tensor of diagonal scale whose only off-diagonal makes one order-two minor negative by
// exactly margin times the square of the scale.
meios::inertia<double> skewed(double scale, double margin)
{
    const double off = scale * std::sqrt(1.0 + margin);
    return meios::inertia<double>{ scale, off, 0.0, scale, 0.0, scale };
}

}

TEST_CASE("a negative mass and a negative moment carry different codes", "[urdf][inertia]")
{
    const meios::expected<meios::load_result, meios::load_error> mass =
        load_profile("inertia_negative_mass.urdf");
    const meios::expected<meios::load_result, meios::load_error> moment =
        load_profile("inertia_negative_diagonal.urdf");

    REQUIRE_FALSE(mass);
    REQUIRE_FALSE(moment);
    REQUIRE(mass.error().code == meios::diagnostic_code::invalid_mass);
    REQUIRE(moment.error().code == meios::diagnostic_code::invalid_inertia);
    REQUIRE(moment.error().loc.line > 0);
}

TEST_CASE("a tensor only a non-leading principal minor catches is refused", "[urdf][inertia]")
{
    const meios::inertia<double> t{ 0.0, 0.0, 0.0, 1.0, 2.0, 1.0 };

    REQUIRE(t.ixx >= 0.0);
    REQUIRE(t.ixx * t.iyy - t.ixy * t.ixy >= 0.0);
    REQUIRE(t.iyy * t.izz - t.iyz * t.iyz < 0.0);

    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_profile("inertia_not_semidefinite.urdf");
    REQUIRE_FALSE(loaded);
    REQUIRE(loaded.error().code == meios::diagnostic_code::invalid_inertia);
}

TEST_CASE("a tensor violating a triangle inequality is refused", "[urdf][inertia]")
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_profile("inertia_triangle_violation.urdf");

    REQUIRE_FALSE(loaded);
    REQUIRE(loaded.error().code == meios::diagnostic_code::invalid_inertia);
}

TEST_CASE("a wholly zero tensor paired with a zero mass is the massless frame convention",
          "[urdf][inertia]")
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_profile("inertia_zero_massless.urdf");

    REQUIRE(loaded);
    const meios::link<double> &frame = loaded->robot.links.at(0);
    REQUIRE(frame.body.has_value());
    REQUIRE(frame.body->mass == 0.0);
    REQUIRE(frame.body->tensor.izz == 0.0);
}

TEST_CASE("a zero diagonal on a tensor that is not wholly zero is a malformed tensor",
          "[urdf][inertia]")
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_profile("inertia_degenerate_scale.urdf");

    REQUIRE_FALSE(loaded);
    REQUIRE(loaded.error().code == meios::diagnostic_code::invalid_inertia);
}

TEST_CASE("the accept and refuse split lands where the profile publishes it", "[urdf][inertia]")
{
    REQUIRE(load_profile("inertia_within_tolerance.urdf"));
    REQUIRE_FALSE(load_profile("inertia_outside_tolerance.urdf"));
    REQUIRE(load_profile("inertia_outside_tolerance.urdf").error().code
            == meios::diagnostic_code::invalid_inertia);
}

TEST_CASE("the same relative margin decides at either end of the scale range", "[urdf][inertia]")
{
    checker check;
    for(const double scale : { 1e-6, 1.0, 1e6 })
    {
        INFO(scale);
        REQUIRE(check.accepts(1.0, skewed(scale, 1e-12)));
        REQUIRE_FALSE(check.accepts(1.0, skewed(scale, 1e-6)));
    }
}

// An infinite diagonal entry makes the scale infinite, and every tolerance derived from it
// infinite too, so every comparison downstream succeeds; deciding finiteness first is what
// keeps such a tensor from being accepted rather than refused.
TEST_CASE("a non-finite value is refused before any product is formed", "[urdf][inertia]")
{
    checker check;
    const double big = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const meios::inertia<double> sane{ 1.0, 0.0, 0.0, 1.0, 0.0, 1.0 };

    REQUIRE_FALSE(check.accepts(big, sane));
    REQUIRE_FALSE(check.accepts(nan, sane));
    REQUIRE_FALSE(check.accepts(1.0, meios::inertia<double>{ big, 0.0, 0.0, 1.0, 0.0, 1.0 }));
    REQUIRE_FALSE(check.accepts(1.0, meios::inertia<double>{ 1.0, nan, 0.0, 1.0, 0.0, 1.0 }));
}

TEST_CASE("a real tensor sits orders of magnitude inside the boundary", "[urdf][inertia]")
{
    checker check;
    const meios::inertia<double> upper_arm{ 0.056709, 0.000292, -0.054852,
                                            2.44012,  -0.000014, 2.432744 };

    REQUIRE(check.accepts(8.393, upper_arm));
}

#include "profile_table.h"
#include "profile_fixture.h"

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/claims.h>
#include <meios/diagnostic/completeness.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{

bool refused_by_a_published_rule(meios::diagnostic_code code)
{
    const std::string spelling{ meios::to_string(code) };
    for(const profile::row &r : profile::load_rows())
    {
        if(r.verdict != "accept" && r.code == spelling)
            return true;
    }
    return false;
}

}

TEST_CASE("the rule table reaches the runner whole", "[urdf][profile]")
{
    const std::vector<profile::row> rows = profile::load_rows();
    REQUIRE(rows.size() >= 3);
    for(const profile::row &r : rows)
    {
        INFO(r.fixture);
        REQUIRE(profile::declared_verdict(r.verdict));
        REQUIRE_FALSE(r.rule.empty());
    }
}

TEST_CASE("every refusing row is refused under its own code at a real location", "[urdf][profile]")
{
    for(const profile::row &r : profile::load_rows())
    {
        if(r.verdict != "refuse")
            continue;
        INFO(r.fixture);
        REQUIRE(profile::located(profile::load_fixture(r.fixture).diagnostics, meios::level::error,
                                 r.code));
    }
}

TEST_CASE("every dropping row discloses its code, still loads, and answers for the parse claim",
          "[urdf][profile]")
{
    for(const profile::row &r : profile::load_rows())
    {
        if(r.verdict != "drop")
            continue;
        INFO(r.fixture);
        const profile::outcome result = profile::load_fixture(r.fixture);
        REQUIRE(result.succeeded);
        REQUIRE(profile::errors(result.diagnostics) == 0);
        REQUIRE(profile::located(result.diagnostics, meios::level::warn, r.code));
        const meios::completeness cleared = meios::cleared_by(profile::code_from(r.code));
        REQUIRE(meios::has(result.claims, meios::completeness::parsed)
                == !meios::has(cleared, meios::completeness::parsed));
    }
}

TEST_CASE("every accepting row loads without an error diagnostic", "[urdf][profile]")
{
    for(const profile::row &r : profile::load_rows())
    {
        if(r.verdict != "accept")
            continue;
        INFO(r.fixture);
        const profile::outcome result = profile::load_fixture(r.fixture);
        REQUIRE(result.succeeded);
        REQUIRE(profile::errors(result.diagnostics) == 0);
    }
}

// This one document carries many defects at once, which is the opposite shape to the rule
// table's one-rule-per-row, and it is kept whole for exactly that reason: its point is that
// a document this broken once reported success, and splitting it would destroy the
// demonstration while weakening the table's property that a row proves one rule.
TEST_CASE("the document that once loaded clean is refused and names what it reached",
          "[urdf][profile]")
{
    const profile::outcome plain = profile::load_fixture("coercion_probe.urdf");
    REQUIRE_FALSE(plain.succeeded);
    REQUIRE(profile::located(plain.diagnostics, meios::level::error, "unsupported_version"));
    REQUIRE(profile::located(plain.diagnostics, meios::level::warn, "unknown_attribute"));
    REQUIRE(profile::located(plain.diagnostics, meios::level::warn, "unknown_element"));

    meios::load_options permissive;
    permissive.strict = meios::strictness::warn;
    permissive.topology = meios::topology_policy::warn;
    const profile::outcome softened = profile::load_fixture("coercion_probe.urdf", permissive);
    REQUIRE_FALSE(softened.succeeded);
    REQUIRE(profile::located(softened.diagnostics, meios::level::error, "dangling_mimic"));
}

// A load stops at the first refusing class it reaches, so no single document can report
// every defect it carries; each is bound here to the published rule that refuses it and
// to the table row that proves that rule on a document of its own.
TEST_CASE("every defect the refused document carries answers to a published rule",
          "[urdf][profile]")
{
    const std::vector<meios::diagnostic_code> defects = {
        meios::diagnostic_code::unsupported_version,     // version="9.9"
        meios::diagnostic_code::unknown_attribute,       // bogus="yes" on <robot>
        meios::diagnostic_code::vector_arity,            // <origin xyz="1 2"/>
        meios::diagnostic_code::invalid_number,          // <mass value=""/>
        meios::diagnostic_code::invalid_inertia,         // ixx="-5" against a scale of one
        meios::diagnostic_code::unknown_geometry_shape,  // <trapezoid/> under <geometry>
        meios::diagnostic_code::unknown_element,         // <bogus_child/> inside the link
        meios::diagnostic_code::unknown_attribute,       // tpye="revolute" on <joint>
        meios::diagnostic_code::missing_joint_type,      // and so the joint has no type
        meios::diagnostic_code::vector_arity,            // <origin xyz="1 2 3 4"/>
        meios::diagnostic_code::zero_axis,               // <axis xyz="0 0 0"/>
        meios::diagnostic_code::dangling_mimic,          // <mimic joint="does_not_exist"/>
    };
    for(meios::diagnostic_code code : defects)
    {
        INFO(meios::to_string(code));
        REQUIRE(refused_by_a_published_rule(code));
    }
}

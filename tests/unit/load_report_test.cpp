#include "load_failure_fixture.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("a successful load returns every diagnostic the document raised, not the first",
          "[urdf][load_failure]")
{
    // The fixture's three diagnostics span all three claims, and two of the three policies
    // that raise them refuse by default; both are relaxed so the load reaches the success
    // arm this case is about.
    meios::load_options opts;
    opts.topology = meios::topology_policy::warn;
    opts.on_missing = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/three_diagnostics_ok.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE(result->diagnostics.size() >= 3);
}

TEST_CASE("a failing load returns every diagnostic beside the error that names the failure",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/three_diagnostics_fail.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().diagnostics.size() >= 3);
    REQUIRE(result.error().code == meios::diagnostic_code::duplicate_attribute);
    REQUIRE(result.error().code == result.error().diagnostics.front().code);
    REQUIRE(result.error().loc.line == result.error().diagnostics.front().loc.line);
}

TEST_CASE("the returned diagnostic list keeps the order the document raised them",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/three_diagnostics_fail.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    const std::vector<meios::captured_diagnostic> &records = result.error().diagnostics;
    REQUIRE(records.size() >= 3);
    REQUIRE(records[0].loc.line < records[1].loc.line);
    REQUIRE(records[1].loc.line < records[2].loc.line);
}

TEST_CASE("an unresolved asset and a broken topology clear their own claims and no other",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.topology = meios::topology_policy::warn;
    opts.on_missing = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/three_diagnostics_ok.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE(meios::has(result->claims, meios::completeness::parsed));
    REQUIRE_FALSE(meios::has(result->claims, meios::completeness::topology_valid));
    REQUIRE_FALSE(meios::has(result->claims, meios::completeness::deployment_complete));
}

TEST_CASE("a document the reader could not read whole still reports a valid topology",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.strict = meios::strictness::warn;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/three_diagnostics_fail.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE_FALSE(meios::has(result->claims, meios::completeness::parsed));
    REQUIRE(meios::has(result->claims, meios::completeness::topology_valid));
    REQUIRE(meios::has(result->claims, meios::completeness::deployment_complete));
}

TEST_CASE("a clean description returns all three completeness claims", "[urdf][load_failure]")
{
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/origin_xyz_ok.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE(result->diagnostics.empty());
    REQUIRE(meios::has(result->claims, meios::completeness::parsed));
    REQUIRE(meios::has(result->claims, meios::completeness::topology_valid));
    REQUIRE(meios::has(result->claims, meios::completeness::deployment_complete));
}

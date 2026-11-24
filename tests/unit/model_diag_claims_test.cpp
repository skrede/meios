#include <meios/diagnostic/level.h>
#include <meios/diagnostic/claims.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/completeness.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/capturing_log_sink.h>
#include <meios/diagnostic/captured_diagnostic.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("completeness claims are independent of one another", "[model][diag]")
{
    constexpr meios::completeness partial = meios::completeness::deployment_complete | meios::completeness::parsed;

    REQUIRE(meios::has(partial, meios::completeness::parsed));
    REQUIRE(meios::has(partial, meios::completeness::deployment_complete));
    REQUIRE_FALSE(meios::has(partial, meios::completeness::topology_valid));
}

TEST_CASE("a completeness query naming two claims asks for both", "[model][diag]")
{
    constexpr meios::completeness both = meios::completeness::parsed | meios::completeness::topology_valid;

    static_assert(meios::has(both, both));
    REQUIRE(meios::has(both, meios::completeness::parsed));
    REQUIRE_FALSE(meios::has(meios::completeness::parsed, both));
}

TEST_CASE("the empty completeness value carries no claim", "[model][diag]")
{
    REQUIRE_FALSE(meios::has(meios::completeness::none, meios::completeness::parsed));
    REQUIRE_FALSE(meios::has(meios::completeness::none, meios::completeness::topology_valid));
    REQUIRE_FALSE(meios::has(meios::completeness::none, meios::completeness::deployment_complete));
}

TEST_CASE("clearing one completeness claim leaves the other two standing", "[model][diag]")
{
    meios::completeness claims = meios::all_claims;
    claims &= ~meios::completeness::topology_valid;

    REQUIRE(meios::has(claims, meios::completeness::parsed));
    REQUIRE(meios::has(claims, meios::completeness::deployment_complete));
    REQUIRE_FALSE(meios::has(claims, meios::completeness::topology_valid));
}

TEST_CASE("a document that said nothing wrong claims everything", "[model][diag]")
{
    REQUIRE(meios::claims_from({}) == meios::all_claims);
}

TEST_CASE("an unresolved asset clears only the deployment claim", "[model][diag]")
{
    const std::vector<meios::captured_diagnostic> records{{meios::diagnostic_code::unresolved_asset, {}, "missing mesh"}};
    const meios::completeness claims = meios::claims_from(records);

    REQUIRE(meios::has(claims, meios::completeness::parsed));
    REQUIRE(meios::has(claims, meios::completeness::topology_valid));
    REQUIRE_FALSE(meios::has(claims, meios::completeness::deployment_complete));
}

TEST_CASE("a broken kinematic structure clears only the topology claim", "[model][diag]")
{
    const std::vector<meios::captured_diagnostic> records{{meios::diagnostic_code::multiple_parents, {}, "two parents"}};
    const meios::completeness claims = meios::claims_from(records);

    REQUIRE(meios::has(claims, meios::completeness::parsed));
    REQUIRE(meios::has(claims, meios::completeness::deployment_complete));
    REQUIRE_FALSE(meios::has(claims, meios::completeness::topology_valid));
}

TEST_CASE("claims are cleared independently, not progressively", "[model][diag]")
{
    const std::vector<meios::captured_diagnostic> records{{meios::diagnostic_code::unresolved_asset, {}, "missing mesh"},
                                                          {meios::diagnostic_code::no_root_cycle, {}, "every link has a parent"}};
    const meios::completeness claims = meios::claims_from(records);

    REQUIRE(meios::has(claims, meios::completeness::parsed));
    REQUIRE_FALSE(meios::has(claims, meios::completeness::topology_valid));
    REQUIRE_FALSE(meios::has(claims, meios::completeness::deployment_complete));
}

TEST_CASE("a claim answers to what the document said, not to how loudly", "[model][diag]")
{
    meios::log_sink discard;
    meios::capturing_log_sink sink{discard};
    meios::log_sink &seam = sink;
    seam.log(meios::level::warn, meios::diagnostic_code::unresolved_asset, meios::source_location{}, "missing mesh");
    seam.log(meios::level::info, meios::diagnostic_code::link_on_cycle, meios::source_location{}, "link sits on a cycle");
    const meios::completeness claims = meios::claims_from(sink.records());

    REQUIRE(sink.errors() == 0);
    REQUIRE(sink.records().size() == 2);
    REQUIRE(meios::has(claims, meios::completeness::parsed));
    REQUIRE_FALSE(meios::has(claims, meios::completeness::topology_valid));
    REQUIRE_FALSE(meios::has(claims, meios::completeness::deployment_complete));
}

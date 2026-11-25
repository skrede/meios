#include "cli_verbs_fixture.h"
#include "silent_default_fixture.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <fstream>
#include <filesystem>

namespace
{

// The relative form is measured against the input document's own directory, so a document
// whose relative meshes resolve can only be driven from a tree the test built: the committed
// fixture is copied next to the files it names.
std::filesystem::path seed_relative_tree()
{
    const auto stamp                 = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path base = std::filesystem::temp_directory_path() / ("meios_relative_" + std::to_string(stamp));
    std::filesystem::create_directories(base / "meshes");
    std::ofstream(base / "meshes" / "base.stl") << "solid x\nendsolid x\n";
    std::ofstream(base / "meshes" / "upper.stl") << "solid x\nendsolid x\n";
    const std::filesystem::path document = base / "relative_asset_present.urdf";
    std::filesystem::copy_file(fixture("profile/relative_asset_present.urdf"), document);
    return document;
}

meios::diagnostic_code refusal_code(const std::string &name, const meios::load_options &opts)
{
    const meios::expected<meios::load_result, meios::load_error> result = meios::load(fixture(name), opts);
    REQUIRE_FALSE(result.has_value());
    return result.error().code;
}

}

TEST_CASE("options nobody configured refuse a document whose relative meshes do not resolve", "[urdf][silent][asset]")
{
    // This case assigns no policy on purpose. Setting the asset policy here would test the
    // enumerator rather than the default, and the case would keep passing if the default
    // were put back to warning — which is the regression it exists to catch.
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result = meios::load(fixture("profile/relative_asset_absent.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_asset);
}

TEST_CASE("a deliberately permissive asset policy withholds the deployment claim from relative "
          "meshes that do not resolve",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    opts.on_missing                                                     = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result = meios::load(fixture("profile/relative_asset_absent.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE_FALSE(meios::has(result->claims, meios::completeness::deployment_complete));
    REQUIRE(meios::has(result->claims, meios::completeness::parsed));
    REQUIRE(meios::has(result->claims, meios::completeness::topology_valid));
}

TEST_CASE("a document whose relative meshes all resolve keeps the deployment claim", "[urdf][silent][asset]")
{
    const std::filesystem::path document = seed_relative_tree();

    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result = meios::load(document, opts);

    std::filesystem::remove_all(document.parent_path());

    REQUIRE(result.has_value());
    REQUIRE(meios::has(result->claims, meios::completeness::deployment_complete));
}

TEST_CASE("the deliberately permissive path names every relative mesh that did not resolve, "
          "one diagnostic each",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    opts.on_missing                                                     = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result = meios::load(fixture("profile/relative_asset_absent.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE(count_code(result->diagnostics, meios::diagnostic_code::unresolved_asset) == 2);
    REQUIRE(mentions(result->diagnostics, "absent_meshes/base.stl"));
    REQUIRE(mentions(result->diagnostics, "absent_meshes/upper.stl"));
}

TEST_CASE("a mesh climbing out of every root is refused at the default, and a deliberately "
          "lowered asset policy does not soften it",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    REQUIRE(refusal_code("profile/relative_asset_climb_out.urdf", opts) == meios::diagnostic_code::uncontained_asset);

    opts.on_missing = meios::missing_asset::skip;
    REQUIRE(refusal_code("profile/relative_asset_climb_out.urdf", opts) == meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("a mesh naming an unsupported scheme is refused at the default, and a deliberately "
          "lowered asset policy does not soften it",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    REQUIRE(refusal_code("profile/unsupported_scheme_asset.urdf", opts) == meios::diagnostic_code::unsupported_uri_scheme);

    opts.on_missing = meios::missing_asset::skip;
    REQUIRE(refusal_code("profile/unsupported_scheme_asset.urdf", opts) == meios::diagnostic_code::unsupported_uri_scheme);
}

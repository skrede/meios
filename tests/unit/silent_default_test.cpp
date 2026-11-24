#include "cli_verbs_fixture.h"
#include "silent_default_fixture.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <fstream>
#include <utility>
#include <filesystem>

namespace
{

struct silent_load
{
    meios::expected<meios::load_result, meios::load_error> result;
    std::string cerr_bytes;
    std::string cout_bytes;
};

silent_load load_quietly(const std::string &name)
{
    cerr_capture err;
    cout_capture out;
    meios::expected<meios::load_result, meios::load_error> result = meios::load(fixture(name), meios::load_options{});
    return silent_load{std::move(result), err.str(), out.str()};
}

std::filesystem::path make_pkg_root()
{
    const auto stamp           = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("meios_silent_" + std::to_string(stamp));
    std::filesystem::create_directories(root / "somepkg" / "meshes");
    std::ofstream(root / "somepkg" / "meshes" / "x.stl") << "solid x\nendsolid x\n";
    return root;
}

}

TEST_CASE("a default load writes nothing to cerr or cout", "[urdf][silent]")
{
    const silent_load quiet = load_quietly("test-desc.urdf");

    REQUIRE(quiet.result.has_value());
    REQUIRE(quiet.cerr_bytes.empty());
    REQUIRE(quiet.cout_bytes.empty());
}

TEST_CASE("options nobody configured refuse a document naming an unresolvable asset", "[urdf][silent][asset]")
{
    // This case assigns no policy on purpose. Setting the asset policy here would test the
    // enumerator rather than the default, and the case would keep passing if the default
    // were put back to warning — which is the regression it exists to catch.
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result = meios::load(fixture("profile/unresolved_asset.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_asset);
}

TEST_CASE("a document whose assets all resolve keeps the deployment claim", "[urdf][silent][asset]")
{
    const std::filesystem::path root = make_pkg_root();

    meios::load_options opts;
    opts.package_roots                                                  = {root};
    const meios::expected<meios::load_result, meios::load_error> result = meios::load(fixture("package_mesh.urdf"), opts);

    std::filesystem::remove_all(root);

    REQUIRE(result.has_value());
    REQUIRE(meios::has(result->claims, meios::completeness::deployment_complete));
}

TEST_CASE("a deliberately permissive asset policy yields a value that withholds only the "
          "deployment claim",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    opts.on_missing                                                     = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result = meios::load(fixture("profile/unresolved_asset.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE_FALSE(meios::has(result->claims, meios::completeness::deployment_complete));
    REQUIRE(meios::has(result->claims, meios::completeness::parsed));
    REQUIRE(meios::has(result->claims, meios::completeness::topology_valid));
}

TEST_CASE("the permissive path names every asset that did not resolve, one diagnostic each", "[urdf][silent][asset]")
{
    meios::load_options opts;
    opts.on_missing                                                     = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result = meios::load(fixture("profile/unresolved_asset.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE(count_code(result->diagnostics, meios::diagnostic_code::unresolved_asset) == 2);
    REQUIRE(mentions(result->diagnostics, "absent_pkg/meshes/base.stl"));
    REQUIRE(mentions(result->diagnostics, "other_absent_pkg/meshes/upper.stl"));
}

TEST_CASE("options nobody configured refuse a document whose texture does not resolve", "[urdf][silent][asset]")
{
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result = meios::load(fixture("profile/unresolved_texture.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_asset);
}

TEST_CASE("to_string covers the trace and debug levels", "[diagnostic][level]")
{
    REQUIRE(meios::to_string(meios::level::trace) == "trace");
    REQUIRE(meios::to_string(meios::level::debug) == "debug");
}

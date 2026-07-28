#include <meios/urdf.h>
#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <streambuf>
#include <filesystem>
#include <string_view>

namespace
{

std::filesystem::path fixture(const std::string &name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

std::filesystem::path make_pkg_root()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("meios_silent_" + std::to_string(stamp));
    std::filesystem::create_directories(root / "somepkg" / "meshes");
    std::ofstream(root / "somepkg" / "meshes" / "x.stl") << "solid x\nendsolid x\n";
    return root;
}

int count_code(const std::vector<meios::captured_diagnostic> &records, meios::diagnostic_code code)
{
    int n = 0;
    for(const meios::captured_diagnostic &record : records)
        if(record.code == code)
            ++n;
    return n;
}

bool mentions(const std::vector<meios::captured_diagnostic> &records, std::string_view needle)
{
    for(const meios::captured_diagnostic &record : records)
        if(record.message.find(needle) != std::string::npos)
            return true;
    return false;
}

}

TEST_CASE("a default load writes nothing to cerr or cout", "[urdf][silent]")
{
    std::ostringstream cerr_capture;
    std::ostringstream cout_capture;
    std::streambuf *saved_cerr = std::cerr.rdbuf(cerr_capture.rdbuf());
    std::streambuf *saved_cout = std::cout.rdbuf(cout_capture.rdbuf());

    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("test-desc.urdf"), opts);

    std::cerr.rdbuf(saved_cerr);
    std::cout.rdbuf(saved_cout);

    REQUIRE(result.has_value());
    REQUIRE(cerr_capture.str().empty());
    REQUIRE(cout_capture.str().empty());
}

TEST_CASE("options nobody configured refuse a document naming an unresolvable asset",
          "[urdf][silent][asset]")
{
    // This case assigns no policy on purpose. Setting the asset policy here would test the
    // enumerator rather than the default, and the case would keep passing if the default
    // were put back to warning — which is the regression it exists to catch.
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/unresolved_asset.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_asset);
}

TEST_CASE("a document whose assets all resolve keeps the deployment claim", "[urdf][silent][asset]")
{
    const std::filesystem::path root = make_pkg_root();

    meios::load_options opts;
    opts.package_roots = { root };
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("package_mesh.urdf"), opts);

    std::filesystem::remove_all(root);

    REQUIRE(result.has_value());
    REQUIRE(meios::has(result->claims, meios::completeness::deployment_complete));
}

TEST_CASE("a deliberately permissive asset policy yields a value that withholds only the "
          "deployment claim",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/unresolved_asset.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE_FALSE(meios::has(result->claims, meios::completeness::deployment_complete));
    REQUIRE(meios::has(result->claims, meios::completeness::parsed));
    REQUIRE(meios::has(result->claims, meios::completeness::topology_valid));
}

TEST_CASE("the permissive path names every asset that did not resolve, one diagnostic each",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/unresolved_asset.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE(count_code(result->diagnostics, meios::diagnostic_code::unresolved_asset) == 2);
    REQUIRE(mentions(result->diagnostics, "absent_pkg/meshes/base.stl"));
    REQUIRE(mentions(result->diagnostics, "other_absent_pkg/meshes/upper.stl"));
}

TEST_CASE("to_string covers the trace and debug levels", "[diagnostic][level]")
{
    REQUIRE(meios::to_string(meios::level::trace) == "trace");
    REQUIRE(meios::to_string(meios::level::debug) == "debug");
}

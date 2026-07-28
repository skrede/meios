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

// The relative form is measured against the input document's own directory, so a document
// whose relative meshes resolve can only be driven from a tree the test built: the committed
// fixture is copied next to the files it names.
std::filesystem::path seed_relative_tree()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path base =
        std::filesystem::temp_directory_path() / ("meios_relative_" + std::to_string(stamp));
    std::filesystem::create_directories(base / "meshes");
    std::ofstream(base / "meshes" / "base.stl") << "solid x\nendsolid x\n";
    std::ofstream(base / "meshes" / "upper.stl") << "solid x\nendsolid x\n";
    const std::filesystem::path document = base / "relative_asset_present.urdf";
    std::filesystem::copy_file(fixture("profile/relative_asset_present.urdf"), document);
    return document;
}

meios::diagnostic_code refusal_code(const std::string &name, const meios::load_options &opts)
{
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture(name), opts);
    REQUIRE_FALSE(result.has_value());
    return result.error().code;
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

TEST_CASE("options nobody configured refuse a document whose relative meshes do not resolve",
          "[urdf][silent][asset]")
{
    // This case assigns no policy on purpose. Setting the asset policy here would test the
    // enumerator rather than the default, and the case would keep passing if the default
    // were put back to warning — which is the regression it exists to catch.
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/relative_asset_absent.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_asset);
}

TEST_CASE("a deliberately permissive asset policy withholds the deployment claim from relative "
          "meshes that do not resolve",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/relative_asset_absent.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE_FALSE(meios::has(result->claims, meios::completeness::deployment_complete));
    REQUIRE(meios::has(result->claims, meios::completeness::parsed));
    REQUIRE(meios::has(result->claims, meios::completeness::topology_valid));
}

TEST_CASE("a document whose relative meshes all resolve keeps the deployment claim",
          "[urdf][silent][asset]")
{
    const std::filesystem::path document = seed_relative_tree();

    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(document, opts);

    std::filesystem::remove_all(document.parent_path());

    REQUIRE(result.has_value());
    REQUIRE(meios::has(result->claims, meios::completeness::deployment_complete));
}

TEST_CASE("the deliberately permissive path names every relative mesh that did not resolve, "
          "one diagnostic each",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/relative_asset_absent.urdf"), opts);

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
    REQUIRE(refusal_code("profile/relative_asset_climb_out.urdf", opts)
            == meios::diagnostic_code::uncontained_asset);

    opts.on_missing = meios::missing_asset::skip;
    REQUIRE(refusal_code("profile/relative_asset_climb_out.urdf", opts)
            == meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("a mesh naming an unsupported scheme is refused at the default, and a deliberately "
          "lowered asset policy does not soften it",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    REQUIRE(refusal_code("profile/unsupported_scheme_asset.urdf", opts)
            == meios::diagnostic_code::unsupported_uri_scheme);

    opts.on_missing = meios::missing_asset::skip;
    REQUIRE(refusal_code("profile/unsupported_scheme_asset.urdf", opts)
            == meios::diagnostic_code::unsupported_uri_scheme);
}

TEST_CASE("options nobody configured refuse a document whose texture does not resolve",
          "[urdf][silent][asset]")
{
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("profile/unresolved_texture.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_asset);
}

TEST_CASE("to_string covers the trace and debug levels", "[diagnostic][level]")
{
    REQUIRE(meios::to_string(meios::level::trace) == "trace");
    REQUIRE(meios::to_string(meios::level::debug) == "debug");
}

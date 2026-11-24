#include "urdf_mesh_fixture.h"

#include <meios/io.h>
#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <optional>
#include <algorithm>
#include <filesystem>

namespace
{

struct recorder
{
    std::vector<meios::level> &levels;

    void operator()(meios::level lvl, const std::string &)
    {
        levels.push_back(lvl);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &)
    {
        levels.push_back(lvl);
    }
};

}

TEST_CASE("a failed scratch write refuses the load at every missing_asset setting", "[urdf][mesh]")
{
    meios::log_sink inner;
    meios::capturing_log_sink log{inner};
    meios::memory_source memory{log};
    // An archive can legitimately hold both an entry named "meshes" and an entry named
    // "meshes/x.stl", so writing the first as a regular file and then asking for the second
    // is a real failure mode rather than a contrivance.
    memory.add("somepkg", "meshes", "not-a-directory\n");
    memory.add("somepkg", "meshes/x.stl", "solid\n");
    REQUIRE(memory.locate("somepkg", "meshes").has_value());
    meios::source_stack sources{std::move(memory)};

    for(meios::missing_asset policy : {meios::missing_asset::skip, meios::missing_asset::warn, meios::missing_asset::fail})
    {
        INFO("missing_asset " << static_cast<int>(policy));
        meios::load_options opts;
        opts.on_missing                                                     = policy;
        const meios::expected<meios::load_result, meios::load_error> result = meios::load(mesh_fixture(), opts, sources, log);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == meios::diagnostic_code::asset_write_failed);
    }
}

TEST_CASE("a resolved mesh that is an unsmudged Git-LFS pointer is rejected, not accepted", "[urdf][mesh]")
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "meios_urdf_lfs_fixture";
    std::filesystem::create_directories(root / "somepkg" / "meshes");
    std::ofstream(root / "somepkg" / "meshes" / "x.stl") << "version https://git-lfs.github.com/spec/v1\n"
                                                            "oid sha256:4d7a2146e8f0a9c1b2d3e4f5061728394a5b6c7d8e9f0a1b2c3d4e5f60718293\n"
                                                            "size 12345\n";

    meios::log_sink inner;
    meios::capturing_log_sink capture{inner};
    meios::directory_source dir{root, capture};
    meios::source_stack sources{dir};
    const meios::tree<double> robot = parse_document("package_mesh.urdf", sources, meios::missing_asset::warn, capture);

    const std::optional<meios::captured_diagnostic> refusal = capture.first();
    REQUIRE_FALSE(first_mesh(robot).resolved_path.has_value());
    REQUIRE(refusal.has_value());
    REQUIRE(refusal->code == meios::diagnostic_code::lfs_pointer_asset);
    std::filesystem::remove_all(root);
}

TEST_CASE("a package:// URI naming no path within the package is refused as malformed", "[urdf][mesh]")
{
    meios::log_sink inner;
    meios::capturing_log_sink capture{inner};
    meios::source_stack sources{};
    const meios::tree<double> robot = parse_document("malformed_package_uri.urdf", sources, meios::missing_asset::warn, capture);

    const std::optional<meios::captured_diagnostic> refusal = capture.first();
    REQUIRE_FALSE(first_mesh(robot).resolved_path.has_value());
    REQUIRE(refusal.has_value());
    REQUIRE(refusal->code == meios::diagnostic_code::malformed_asset_uri);
}

TEST_CASE("a missing mesh honors the missing_asset policy", "[urdf][mesh]")
{
    SECTION("fail logs an error and leaves the path empty")
    {
        std::vector<meios::level> levels;
        meios::log_sink_f capture{recorder{levels}};
        meios::source_stack sources{};
        const meios::tree<double> robot = parse_document("package_mesh.urdf", sources, meios::missing_asset::fail, capture);

        REQUIRE_FALSE(first_mesh(robot).resolved_path.has_value());
        REQUIRE(std::count(levels.begin(), levels.end(), meios::level::error) >= 1);
    }

    SECTION("warn logs a warning and leaves the path empty")
    {
        std::vector<meios::level> levels;
        meios::log_sink_f capture{recorder{levels}};
        meios::source_stack sources{};
        const meios::tree<double> robot = parse_document("package_mesh.urdf", sources, meios::missing_asset::warn, capture);

        REQUIRE_FALSE(first_mesh(robot).resolved_path.has_value());
        REQUIRE(std::count(levels.begin(), levels.end(), meios::level::warn) >= 1);
    }

    SECTION("skip stays silent and leaves the path empty")
    {
        std::vector<meios::level> levels;
        meios::log_sink_f capture{recorder{levels}};
        meios::source_stack sources{};
        const meios::tree<double> robot = parse_document("package_mesh.urdf", sources, meios::missing_asset::skip, capture);

        REQUIRE_FALSE(first_mesh(robot).resolved_path.has_value());
        REQUIRE(levels.empty());
    }
}

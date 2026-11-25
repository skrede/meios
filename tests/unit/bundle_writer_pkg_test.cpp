#include "bundle_writer_pkg_fixture.h"

#include <meios/bundle.h>
#include <meios/model.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace
{

struct code_recorder
{
    std::vector<meios::diagnostic_code> &codes;

    void operator()(meios::level, const std::string &)
    {
        codes.push_back(meios::diagnostic_code::unspecified);
    }

    void operator()(meios::level, const meios::source_location &, const std::string &)
    {
        codes.push_back(meios::diagnostic_code::unspecified);
    }

    void operator()(meios::level, meios::diagnostic_code code, const meios::source_location &, const std::string &)
    {
        codes.push_back(code);
    }
};

bool carries(const std::vector<meios::diagnostic_code> &codes, meios::diagnostic_code wanted)
{
    return std::find(codes.begin(), codes.end(), wanted) != codes.end();
}

}

TEST_CASE("the folder writer copies an asset under the layout and writes the URDF", "[bundle][pkg]")
{
    const std::filesystem::path root = scratch_root("copy");
    const std::filesystem::path src  = write_file(root.parent_path() / "src_base.stl", "MESH-BYTES");
    meios::log_sink silent;
    meios::asset_manifest manifest;
    manifest.entries.push_back(meios::bundle_entry{"package://rob/meshes/rob/base.stl", src, "meshes/rob/base.stl"});

    meios::package_writer writer(root, silent);
    const meios::emit_result result = writer.write("rob.urdf", "<robot name=\"rob\"/>", manifest, false);

    REQUIRE(result.status == meios::emit_status::ok);
    REQUIRE(std::filesystem::exists(root / "meshes/rob/base.stl"));
    REQUIRE(read_file(root / "meshes/rob/base.stl") == "MESH-BYTES");
    REQUIRE(read_file(root / "rob.urdf") == "<robot name=\"rob\"/>");
    std::filesystem::remove_all(root);
    std::filesystem::remove(src);
}

TEST_CASE("a destination escaping the bundle root is rejected with no write", "[bundle][pkg]")
{
    const std::filesystem::path root = scratch_root("escape");
    const std::filesystem::path src  = write_file(root.parent_path() / "src_evil.stl", "X");
    std::filesystem::create_directories(root);
    std::vector<meios::diagnostic_code> codes;
    meios::log_sink_f log{code_recorder{codes}};
    meios::asset_manifest manifest;
    manifest.entries.push_back(meios::bundle_entry{"package://rob/x", src, "../escape.stl"});

    meios::package_writer writer(root, log);
    const meios::emit_result result = writer.write("rob.urdf", "<robot/>", manifest, false);

    REQUIRE(result.status == meios::emit_status::io_error);
    REQUIRE(carries(codes, meios::diagnostic_code::uncontained_asset));
    REQUIRE_FALSE(std::filesystem::exists(root.parent_path() / "escape.stl"));
    std::filesystem::remove_all(root);
    std::filesystem::remove(src);
}

TEST_CASE("a relative output root writes under itself and still refuses an escape", "[bundle][pkg]")
{
    const std::filesystem::path root   = scratch_root("relative");
    const std::filesystem::path src    = write_file(root.parent_path() / "src_rel.stl", "REL-BYTES");
    const std::filesystem::path escape = root.parent_path() / "escape_rel.stl";
    std::filesystem::remove(escape);
    std::filesystem::create_directories(root);
    std::vector<meios::diagnostic_code> codes;
    meios::log_sink_f log{code_recorder{codes}};
    meios::asset_manifest manifest;
    manifest.entries.push_back(meios::bundle_entry{"package://rob/meshes/rob/base.stl", src, "meshes/rob/base.stl"});
    manifest.entries.push_back(meios::bundle_entry{"package://rob/x", src, "../escape_rel.stl"});

    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(root.parent_path());
    meios::package_writer writer(root.filename(), log);
    const meios::emit_result result = writer.write("rob.urdf", "<robot name=\"rob\"/>", manifest, false);
    std::filesystem::current_path(saved);

    const bool copied  = std::filesystem::exists(root / "meshes/rob/base.stl");
    const bool wrote   = read_file(root / "rob.urdf") == "<robot name=\"rob\"/>";
    const bool escaped = std::filesystem::exists(escape);
    std::filesystem::remove_all(root);
    std::filesystem::remove(src);

    REQUIRE(copied);
    REQUIRE(wrote);
    REQUIRE(result.status == meios::emit_status::io_error);
    REQUIRE(carries(codes, meios::diagnostic_code::uncontained_asset));
    REQUIRE_FALSE(escaped);
}

TEST_CASE("dry_run performs no disk mutation", "[bundle][pkg]")
{
    const std::filesystem::path root = scratch_root("dry");
    const std::filesystem::path src  = write_file(root.parent_path() / "src_dry.stl", "X");
    meios::log_sink silent;
    meios::asset_manifest manifest;
    manifest.entries.push_back(meios::bundle_entry{"package://rob/meshes/rob/base.stl", src, "meshes/rob/base.stl"});

    meios::package_writer writer(root, silent);
    const meios::emit_result result = writer.write("rob.urdf", "<robot/>", manifest, true);

    REQUIRE(result.status == meios::emit_status::ok);
    REQUIRE(result.assets_seen == 1);
    REQUIRE_FALSE(std::filesystem::exists(root));
    std::filesystem::remove(src);
}

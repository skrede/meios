#include "zip_writer_fixture.h"

#include <meios/archive/zip_writer.h>
#include <meios/bundle.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <optional>
#include <filesystem>

namespace
{

using zip_fixture::write_file;
using zip_fixture::read_archive;
using zip_fixture::scratch_root;

meios::tree<double> mesh_robot(std::string filename, std::optional<std::string> resolved)
{
    meios::mesh<double> shape{ std::move(filename), { 1.0, 1.0, 1.0 }, std::move(resolved) };
    meios::visual<double> vis{ {}, meios::geometry<double>{ std::move(shape) }, std::nullopt };
    meios::link<double> body{ "base", std::nullopt, { std::move(vis) }, {}, {}, std::nullopt };
    meios::tree<double> robot;
    robot.name = "rob";
    robot.links.push_back(std::move(body));
    return robot;
}

meios::model<double> mesh_model(const std::string &filename, const std::string &resolved)
{
    meios::mesh<double> shape{ filename, { 1.0, 1.0, 1.0 }, resolved };
    meios::visual<double> vis{ {}, meios::geometry<double>{ std::move(shape) }, std::nullopt };
    meios::link<double> body{ "base", std::nullopt, { std::move(vis) }, {}, {}, std::nullopt };
    meios::model<double> robot{};
    robot.name = "rob";
    robot.links.push_back(std::move(body));
    return robot;
}

}

TEST_CASE("the zip writer stores the URDF and an asset that read back", "[archive][zip]")
{
    const std::filesystem::path root = scratch_root("store");
    const std::filesystem::path archive = root / "rob.zip";
    const std::filesystem::path src = write_file(root / "src_base.stl", "MESH-BYTES");
    meios::log_sink silent;
    meios::asset_manifest manifest;
    manifest.entries.push_back(
        meios::bundle_entry{ "package://rob/meshes/rob/base.stl", src, "meshes/rob/base.stl" });

    meios::zip_writer writer(archive, silent);
    const meios::emit_result result = writer.write("rob.urdf", "<robot name=\"rob\"/>", manifest, false);

    REQUIRE(result.status == meios::emit_status::ok);
    const std::map<std::string, std::string> entries = read_archive(archive);
    REQUIRE(entries.at("rob.urdf") == "<robot name=\"rob\"/>");
    REQUIRE(entries.at("meshes/rob/base.stl") == "MESH-BYTES");
    std::filesystem::remove_all(root);
}

TEST_CASE("dry_run creates no archive and reports the same counts as a real write", "[archive][zip]")
{
    const std::filesystem::path root = scratch_root("dry");
    const std::filesystem::path archive = root / "rob.zip";
    const std::filesystem::path src = write_file(root / "src_dry.stl", "X");
    meios::log_sink silent;
    meios::asset_manifest manifest;
    manifest.entries.push_back(
        meios::bundle_entry{ "package://rob/meshes/rob/base.stl", src, "meshes/rob/base.stl" });

    meios::zip_writer writer(archive, silent);
    const meios::emit_result result = writer.write("rob.urdf", "<robot/>", manifest, true);

    REQUIRE(result.status == meios::emit_status::ok);
    REQUIRE(result.assets_seen == 1);
    REQUIRE_FALSE(std::filesystem::exists(archive));
    std::filesystem::remove_all(root);
}

TEST_CASE("bundle_to_zip mirrors bundle_to_folder over a real tree", "[archive][zip]")
{
    const std::filesystem::path root = scratch_root("real");
    const std::filesystem::path archive = root / "rob.zip";
    const std::filesystem::path src = write_file(root / "asset_base.stl", "REAL-MESH");
    meios::log_sink silent;
    meios::source_stack zip_sources;
    meios::source_stack folder_sources;
    meios::scanner_registry registry;
    const meios::tree<double> robot = mesh_robot("package://rob/meshes/base.stl", src.string());
    const meios::zip_request request{ archive, "rob", meios::collision_options{ false },
                                      meios::default_zip_compression, false };

    meios::emit_result out{ meios::emit_status::ok, 0, 0 };
    const meios::asset_manifest manifest =
        meios::bundle_to_zip(robot, zip_sources, registry, request, out, silent);

    meios::emit_result folder_out{ meios::emit_status::ok, 0, 0 };
    const meios::asset_manifest folder = meios::bundle_to_folder(
        robot, folder_sources, registry,
        { root / "folder", "rob", meios::collision_options{ false }, false }, folder_out, silent);

    REQUIRE(out.status == meios::emit_status::ok);
    REQUIRE(out.assets_seen == 1);
    REQUIRE(out.unresolved == 0);
    REQUIRE(manifest.entries.size() == folder.entries.size());
    REQUIRE(manifest.entries[0].dest_relative == folder.entries[0].dest_relative);
    REQUIRE(manifest.entries[0].rewritten_uri == folder.entries[0].rewritten_uri);

    const std::map<std::string, std::string> entries = read_archive(archive);
    REQUIRE(entries.at("rob.urdf").find("package://rob/meshes/rob/meshes/base.stl") != std::string::npos);
    REQUIRE(entries.at(manifest.entries[0].dest_relative) == "REAL-MESH");
    std::filesystem::remove_all(root);
}

TEST_CASE("bundle_to_zip drives the model overload", "[archive][zip]")
{
    const std::filesystem::path root = scratch_root("model");
    const std::filesystem::path archive = root / "rob.zip";
    const std::filesystem::path src = write_file(root / "asset_model.stl", "MODEL-MESH");
    meios::log_sink silent;
    meios::source_stack sources;
    meios::scanner_registry registry;
    const meios::model<double> robot = mesh_model("package://rob/meshes/base.stl", src.string());
    const meios::zip_request request{ archive, "rob", meios::collision_options{ false },
                                      meios::default_zip_compression, false };

    meios::emit_result out{ meios::emit_status::ok, 0, 0 };
    const meios::asset_manifest manifest =
        meios::bundle_to_zip(robot, sources, registry, request, out, silent);

    REQUIRE(out.status == meios::emit_status::ok);
    REQUIRE(manifest.entries.size() == 1);
    const std::map<std::string, std::string> entries = read_archive(archive);
    REQUIRE(entries.at(manifest.entries[0].dest_relative) == "MODEL-MESH");
    std::filesystem::remove_all(root);
}

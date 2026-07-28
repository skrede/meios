#include <meios/bundle.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <optional>
#include <algorithm>
#include <filesystem>

namespace
{

struct recorder
{
    std::vector<std::string> &msgs;

    void operator()(meios::level, const std::string &msg) { msgs.push_back(msg); }

    void operator()(meios::level, const meios::source_location &, const std::string &msg)
    {
        msgs.push_back(msg);
    }
};

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

    void operator()(meios::level, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &)
    {
        codes.push_back(code);
    }
};

bool carries(const std::vector<meios::diagnostic_code> &codes, meios::diagnostic_code wanted)
{
    return std::find(codes.begin(), codes.end(), wanted) != codes.end();
}

std::filesystem::path scratch_root(const std::string &suffix)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / ("meios_pkg_" + suffix);
    std::filesystem::remove_all(root);
    return root;
}

std::filesystem::path write_file(const std::filesystem::path &path, const std::string &text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    return path;
}

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

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

}

TEST_CASE("the folder writer copies an asset under the layout and writes the URDF", "[bundle][pkg]")
{
    const std::filesystem::path root = scratch_root("copy");
    const std::filesystem::path src = write_file(root.parent_path() / "src_base.stl", "MESH-BYTES");
    meios::log_sink silent;
    meios::asset_manifest manifest;
    manifest.entries.push_back(
        meios::bundle_entry{ "package://rob/meshes/rob/base.stl", src, "meshes/rob/base.stl" });

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
    const std::filesystem::path src = write_file(root.parent_path() / "src_evil.stl", "X");
    std::filesystem::create_directories(root);
    std::vector<meios::diagnostic_code> codes;
    meios::log_sink_f log{ code_recorder{ codes } };
    meios::asset_manifest manifest;
    manifest.entries.push_back(
        meios::bundle_entry{ "package://rob/x", src, "../escape.stl" });

    meios::package_writer writer(root, log);
    const meios::emit_result result = writer.write("rob.urdf", "<robot/>", manifest, false);

    REQUIRE(result.status == meios::emit_status::io_error);
    REQUIRE(carries(codes, meios::diagnostic_code::uncontained_asset));
    REQUIRE_FALSE(std::filesystem::exists(root.parent_path() / "escape.stl"));
    std::filesystem::remove_all(root);
    std::filesystem::remove(src);
}

TEST_CASE("dry_run performs no disk mutation", "[bundle][pkg]")
{
    const std::filesystem::path root = scratch_root("dry");
    const std::filesystem::path src = write_file(root.parent_path() / "src_dry.stl", "X");
    meios::log_sink silent;
    meios::asset_manifest manifest;
    manifest.entries.push_back(
        meios::bundle_entry{ "package://rob/meshes/rob/base.stl", src, "meshes/rob/base.stl" });

    meios::package_writer writer(root, silent);
    const meios::emit_result result = writer.write("rob.urdf", "<robot/>", manifest, true);

    REQUIRE(result.status == meios::emit_status::ok);
    REQUIRE(result.assets_seen == 1);
    REQUIRE_FALSE(std::filesystem::exists(root));
    std::filesystem::remove(src);
}

TEST_CASE("a real robot bundles to a folder with every reference rewritten", "[bundle][pkg]")
{
    const std::filesystem::path root = scratch_root("real");
    const std::filesystem::path src = write_file(root.parent_path() / "asset_base.stl", "REAL-MESH");
    meios::log_sink silent;
    meios::source_stack sources;
    meios::scanner_registry registry;
    const meios::tree<double> robot =
        mesh_robot("package://rob/meshes/base.stl", src.string());
    const meios::folder_request request{ root, "rob", meios::collision_options{ false }, false };

    meios::emit_result out{ meios::emit_status::ok, 0, 0 };
    const meios::asset_manifest manifest =
        meios::bundle_to_folder(robot, sources, registry, request, out, silent);

    REQUIRE(out.status == meios::emit_status::ok);
    REQUIRE(out.assets_seen == 1);
    REQUIRE(out.unresolved == 0);
    REQUIRE(manifest.entries.size() == 1);
    REQUIRE(std::filesystem::exists(root / "meshes/rob/meshes/base.stl"));
    REQUIRE(read_file(root / "meshes/rob/meshes/base.stl") == "REAL-MESH");
    REQUIRE(read_file(root / "rob.urdf").find(
                "package://rob/meshes/rob/meshes/base.stl") != std::string::npos);
    std::filesystem::remove_all(root);
    std::filesystem::remove(src);
}

TEST_CASE("the dry-run mutates nothing and returns a manifest equal to the real run", "[bundle][pkg]")
{
    const std::filesystem::path live = scratch_root("equal");
    const std::filesystem::path dry = live.parent_path() / (live.filename().string() + "_dry");
    std::filesystem::remove_all(dry);
    const std::filesystem::path src = write_file(live.parent_path() / "asset_eq.stl", "BYTES");
    meios::log_sink silent;
    meios::source_stack live_sources;
    meios::source_stack dry_sources;
    meios::scanner_registry registry;
    const meios::tree<double> robot = mesh_robot("package://rob/meshes/base.stl", src.string());

    meios::emit_result live_out{ meios::emit_status::ok, 0, 0 };
    meios::emit_result dry_out{ meios::emit_status::ok, 0, 0 };
    const meios::asset_manifest live_manifest = meios::bundle_to_folder(
        robot, live_sources, registry, { live, "rob", meios::collision_options{ false }, false },
        live_out, silent);
    const meios::asset_manifest dry_manifest = meios::bundle_to_folder(
        robot, dry_sources, registry, { dry, "rob", meios::collision_options{ false }, true },
        dry_out, silent);

    REQUIRE_FALSE(std::filesystem::exists(dry));
    REQUIRE(dry_manifest.entries.size() == live_manifest.entries.size());
    REQUIRE(dry_manifest.entries[0].dest_relative == live_manifest.entries[0].dest_relative);
    REQUIRE(dry_manifest.entries[0].rewritten_uri == live_manifest.entries[0].rewritten_uri);
    REQUIRE(dry_manifest.unresolved == live_manifest.unresolved);
    std::filesystem::remove_all(live);
    std::filesystem::remove(src);
}

TEST_CASE("a dry run whose source is missing reports an io error", "[bundle][pkg]")
{
    const std::filesystem::path root = scratch_root("dry_missing");
    meios::log_sink silent;
    meios::asset_manifest manifest;
    manifest.entries.push_back(meios::bundle_entry{
        "package://rob/meshes/rob/base.stl",
        root.parent_path() / "does_not_exist.stl", "meshes/rob/base.stl" });

    meios::package_writer writer(root, silent);
    const meios::emit_result result = writer.write("rob.urdf", "<robot/>", manifest, true);

    REQUIRE(result.status == meios::emit_status::io_error);
    REQUIRE_FALSE(std::filesystem::exists(root));
}

TEST_CASE("an unresolvable mesh reference surfaces in the unresolved list", "[bundle][pkg]")
{
    const std::filesystem::path root = scratch_root("unresolved");
    std::vector<std::string> messages;
    meios::log_sink_f capture{ recorder{ messages } };
    meios::source_stack sources;
    meios::scanner_registry registry;
    const meios::tree<double> robot = mesh_robot("package://ghost/x.stl", std::nullopt);
    const meios::folder_request request{ root, "rob", meios::collision_options{ false }, false };

    meios::emit_result out{ meios::emit_status::ok, 0, 0 };
    const meios::asset_manifest manifest =
        meios::bundle_to_folder(robot, sources, registry, request, out, capture);

    REQUIRE(manifest.entries.empty());
    REQUIRE(manifest.unresolved == std::vector<std::string>{ "package://ghost/x.stl" });
    REQUIRE(out.unresolved == 1);
    bool warned_ghost = false;
    for(const std::string &msg : messages)
        if(msg.find("ghost") != std::string::npos)
            warned_ghost = true;
    REQUIRE(warned_ghost);
    std::filesystem::remove_all(root);
}

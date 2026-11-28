#include "bundle_writer_pkg_fixture.h"

#include <meios/io.h>
#include <meios/model.h>
#include <meios/bundle.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>

namespace
{

struct recorder
{
    std::vector<std::string> &msgs;

    void operator()(meios::level, const std::string &msg)
    {
        msgs.push_back(msg);
    }

    void operator()(meios::level, const meios::source_location &, const std::string &msg)
    {
        msgs.push_back(msg);
    }
};

meios::tree<double> mesh_robot(std::string filename, std::optional<std::string> resolved)
{
    meios::mesh<double> shape{std::move(filename), {1.0, 1.0, 1.0}, std::move(resolved)};
    meios::visual<double> vis{{}, meios::geometry<double>{std::move(shape)}, std::nullopt};
    meios::link<double> body{"base", std::nullopt, {std::move(vis)}, {}, {}, std::nullopt};
    meios::tree<double> robot;
    robot.name = "rob";
    robot.links.push_back(std::move(body));
    return robot;
}

}

TEST_CASE("a real robot bundles to a folder with every reference rewritten", "[bundle][pkg]")
{
    const std::filesystem::path root = scratch_root("real");
    const std::filesystem::path src  = write_file(root.parent_path() / "asset_base.stl", "REAL-MESH");
    meios::log_sink silent;
    meios::source_stack sources;
    meios::scanner_registry registry;
    const meios::tree<double> robot = mesh_robot("package://rob/meshes/base.stl", src.string());
    const meios::folder_request request{root, "rob", meios::collision_options{false}, false};

    meios::emit_result out{meios::emit_status::ok, 0, 0};
    const meios::asset_manifest manifest = meios::bundle_to_folder(robot, sources, registry, request, out, silent);

    REQUIRE(out.status == meios::emit_status::ok);
    REQUIRE(out.assets_seen == 1);
    REQUIRE(out.unresolved == 0);
    REQUIRE(manifest.entries.size() == 1);
    REQUIRE(std::filesystem::exists(root / "meshes/rob/meshes/base.stl"));
    REQUIRE(read_file(root / "meshes/rob/meshes/base.stl") == "REAL-MESH");
    REQUIRE(read_file(root / "rob.urdf").find("package://rob/meshes/rob/meshes/base.stl") != std::string::npos);
    std::filesystem::remove_all(root);
    std::filesystem::remove(src);
}

TEST_CASE("the dry-run mutates nothing and returns a manifest equal to the real run", "[bundle][pkg]")
{
    const std::filesystem::path live = scratch_root("equal");
    const std::filesystem::path dry  = live.parent_path() / (live.filename().string() + "_dry");
    std::filesystem::remove_all(dry);
    const std::filesystem::path src = write_file(live.parent_path() / "asset_eq.stl", "BYTES");
    meios::log_sink silent;
    meios::source_stack live_sources;
    meios::source_stack dry_sources;
    meios::scanner_registry registry;
    const meios::tree<double> robot = mesh_robot("package://rob/meshes/base.stl", src.string());

    meios::emit_result live_out{meios::emit_status::ok, 0, 0};
    meios::emit_result dry_out{meios::emit_status::ok, 0, 0};
    const meios::asset_manifest live_manifest = meios::bundle_to_folder(robot, live_sources, registry, {live, "rob", meios::collision_options{false}, false}, live_out, silent);
    const meios::asset_manifest dry_manifest  = meios::bundle_to_folder(robot, dry_sources, registry, {dry, "rob", meios::collision_options{false}, true}, dry_out, silent);

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
    manifest.entries.push_back(meios::bundle_entry{"package://rob/meshes/rob/base.stl", root.parent_path() / "does_not_exist.stl", "meshes/rob/base.stl"});

    meios::package_writer writer(root, silent);
    const meios::emit_result result = writer.write("rob.urdf", "<robot/>", manifest, true);

    REQUIRE(result.status == meios::emit_status::io_error);
    REQUIRE_FALSE(std::filesystem::exists(root));
}

TEST_CASE("an unresolvable mesh reference surfaces in the unresolved list", "[bundle][pkg]")
{
    const std::filesystem::path root = scratch_root("unresolved");
    std::vector<std::string> messages;
    meios::log_sink_f capture{recorder{messages}};
    meios::source_stack sources;
    meios::scanner_registry registry;
    const meios::tree<double> robot = mesh_robot("package://ghost/x.stl", std::nullopt);
    const meios::folder_request request{root, "rob", meios::collision_options{false}, false};

    meios::emit_result out{meios::emit_status::ok, 0, 0};
    const meios::asset_manifest manifest = meios::bundle_to_folder(robot, sources, registry, request, out, capture);

    REQUIRE(manifest.entries.empty());
    REQUIRE(manifest.unresolved == std::vector<std::string>{"package://ghost/x.stl"});
    REQUIRE(out.unresolved == 1);
    bool warned_ghost = false;
    for(const std::string &msg : messages)
        if(msg.find("ghost") != std::string::npos)
            warned_ghost = true;
    REQUIRE(warned_ghost);
    std::filesystem::remove_all(root);
}

#include "bundle_scanner_fixture.h"

#include <meios/bundle.h>
#include <meios/io.h>
#include <meios/model.h>

#include <meios/bundle/asset_scanner.h>
#include <meios/bundle/scanner_handle.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <filesystem>
#include <type_traits>

namespace
{

meios::resolved_asset make_asset(std::string name)
{
    return meios::resolved_asset{std::filesystem::path{std::move(name)}};
}

}

static_assert(meios::asset_scanner<obj_scanner>);
static_assert(!std::is_copy_constructible_v<meios::scanner_handle>);
static_assert(std::is_move_constructible_v<meios::scanner_handle>);

TEST_CASE("an unknown extension returns the null default", "[bundle][scanner]")
{
    meios::scanner_registry registry;
    meios::log_sink silent;
    meios::resolved_asset asset = make_asset("thing.xyz");

    REQUIRE(registry.scan("xyz", asset, silent).empty());
    REQUIRE(registry.empty());
}

TEST_CASE("a registered scanner is dispatched by its lowercased extension", "[bundle][scanner]")
{
    meios::scanner_registry registry;
    registry.register_scanner("obj", meios::scanner_handle{obj_scanner{{"a.mtl", "b.mtl"}}});
    meios::log_sink silent;
    meios::resolved_asset asset = make_asset("mesh.obj");

    const std::vector<std::string> refs = registry.scan("OBJ", asset, silent);

    REQUIRE(registry.contains("obj"));
    REQUIRE(refs == std::vector<std::string>{"a.mtl", "b.mtl"});
}

TEST_CASE("a package mesh reference rewrites to the namespaced bundle layout", "[bundle][rewrite]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);

    builder.add_reference(mesh_ref("package://ur5_description/meshes/base.stl", "/abs/ur5_description/meshes/base.stl"));

    const meios::asset_manifest &manifest = builder.manifest();
    REQUIRE(manifest.entries.size() == 1);
    REQUIRE(manifest.entries[0].rewritten_uri == "package://botbundle/meshes/ur5_description/meshes/base.stl");
    REQUIRE(manifest.entries[0].dest_relative == "meshes/ur5_description/meshes/base.stl");
    REQUIRE(manifest.entries[0].copy_source == std::filesystem::path("/abs/ur5_description/meshes/base.stl"));
    REQUIRE(builder.status() == meios::emit_status::ok);
}

TEST_CASE("a relative mesh reference buckets under the bundle's own package", "[bundle][rewrite]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);

    builder.add_reference(mesh_ref("meshes/base.stl", "/abs/meshes/base.stl"));

    const meios::asset_manifest &manifest = builder.manifest();
    REQUIRE(manifest.entries.size() == 1);
    REQUIRE(manifest.entries[0].rewritten_uri == "package://botbundle/meshes/botbundle/meshes/base.stl");
    REQUIRE(manifest.entries[0].dest_relative == "meshes/botbundle/meshes/base.stl");
}

TEST_CASE("a malformed package uri loud-fails and adds no entry", "[bundle][rewrite]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);

    builder.add_reference(mesh_ref("package://foo", "/abs/foo"));

    REQUIRE(builder.manifest().entries.empty());
    REQUIRE(builder.status() == meios::emit_status::malformed_reference);
}

TEST_CASE("logically equal source roots are not a collision", "[bundle][collision]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);

    builder.add_reference(mesh_ref("package://ur5/meshes/a.stl", "/ws1/ur5/meshes/a.stl"));
    builder.add_reference(mesh_ref("package://ur5/meshes/b.stl", "/ws1/./ur5/meshes/b.stl"));

    REQUIRE(builder.manifest().entries.size() == 2);
    REQUIRE(builder.manifest().entries[1].dest_relative == "meshes/ur5/meshes/b.stl");
    REQUIRE(builder.status() == meios::emit_status::ok);
}

TEST_CASE("same package name from distinct roots loud-fails by default", "[bundle][collision]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);

    builder.add_reference(mesh_ref("package://ur5/meshes/a.stl", "/ws1/ur5/meshes/a.stl"));
    builder.add_reference(mesh_ref("package://ur5/meshes/b.stl", "/ws2/ur5/meshes/b.stl"));

    REQUIRE(builder.manifest().entries.size() == 1);
    REQUIRE(builder.status() == meios::emit_status::package_collision);
}

std::string second_dir(bool opt_in)
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{opt_in}, sources, silent);
    builder.add_reference(mesh_ref("package://ur5/meshes/a.stl", "/ws1/ur5/meshes/a.stl"));
    builder.add_reference(mesh_ref("package://ur5/meshes/b.stl", "/ws2/ur5/meshes/b.stl"));
    return builder.manifest().entries.back().dest_relative;
}

TEST_CASE("the opt-in hash suffix keeps both colliding packages deterministically", "[bundle][collision]")
{
    const std::string dest = second_dir(true);

    REQUIRE(dest.rfind("meshes/ur5-", 0) == 0);
    REQUIRE(dest == "meshes/ur5-" + dest.substr(11, 8) + "/meshes/b.stl");
    REQUIRE(dest.substr(11, 8).size() == 8);
    REQUIRE(dest == second_dir(true));
}

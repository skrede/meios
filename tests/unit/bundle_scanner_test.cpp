#include <meios/bundle.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <type_traits>
#include <string_view>

namespace
{

struct obj_scanner
{
    std::vector<std::string> refs;

    std::vector<std::string> scan(const meios::resolved_asset &, meios::log_sink &) { return refs; }
};

meios::resolved_asset make_asset(std::string name)
{
    return meios::resolved_asset{ std::filesystem::path{ std::move(name) } };
}

// A path-backed fake source: it answers locate for the "pkg/rel" keys it was seeded
// with, so the closure and the texture path can be driven without touching disk.
struct fake_source
{
    std::map<std::string, std::filesystem::path> table;

    meios::capability_descriptor capabilities() const
    {
        return meios::capability_descriptor{ meios::source_kind::directory, true, false };
    }

    std::optional<meios::resolved_asset> locate(std::string_view pkg, std::string_view rel)
    {
        const std::map<std::string, std::filesystem::path>::iterator hit =
            table.find(std::string(pkg) + '/' + std::string(rel));
        if(hit == table.end())
            return std::nullopt;
        return meios::resolved_asset{ hit->second };
    }
};

meios::reference_record mesh_ref(std::string original, std::optional<std::string> resolved)
{
    return meios::reference_record{ std::move(original), std::move(resolved), false };
}

}

static_assert(meios::asset_scanner<obj_scanner>);
static_assert(meios::package_source<fake_source>);
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
    registry.register_scanner("obj", meios::scanner_handle{ obj_scanner{ { "a.mtl", "b.mtl" } } });
    meios::log_sink silent;
    meios::resolved_asset asset = make_asset("mesh.obj");

    const std::vector<std::string> refs = registry.scan("OBJ", asset, silent);

    REQUIRE(registry.contains("obj"));
    REQUIRE(refs == std::vector<std::string>{ "a.mtl", "b.mtl" });
}

TEST_CASE("a package mesh reference rewrites to the namespaced bundle layout", "[bundle][rewrite]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, silent);

    builder.add_reference(mesh_ref("package://ur5_description/meshes/base.stl",
                                   "/abs/ur5_description/meshes/base.stl"));

    const meios::asset_manifest &manifest = builder.manifest();
    REQUIRE(manifest.entries.size() == 1);
    REQUIRE(manifest.entries[0].rewritten_uri
            == "package://botbundle/meshes/ur5_description/meshes/base.stl");
    REQUIRE(manifest.entries[0].dest_relative == "meshes/ur5_description/meshes/base.stl");
    REQUIRE(manifest.entries[0].copy_source == std::filesystem::path("/abs/ur5_description/meshes/base.stl"));
    REQUIRE(builder.status() == meios::emit_status::ok);
}

TEST_CASE("a relative mesh reference buckets under the bundle's own package", "[bundle][rewrite]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, silent);

    builder.add_reference(mesh_ref("meshes/base.stl", "/abs/meshes/base.stl"));

    const meios::asset_manifest &manifest = builder.manifest();
    REQUIRE(manifest.entries.size() == 1);
    REQUIRE(manifest.entries[0].rewritten_uri
            == "package://botbundle/meshes/botbundle/meshes/base.stl");
    REQUIRE(manifest.entries[0].dest_relative == "meshes/botbundle/meshes/base.stl");
}

TEST_CASE("a malformed package uri loud-fails and adds no entry", "[bundle][rewrite]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, silent);

    builder.add_reference(mesh_ref("package://foo", "/abs/foo"));

    REQUIRE(builder.manifest().entries.empty());
    REQUIRE(builder.status() == meios::emit_status::malformed_reference);
}

TEST_CASE("logically equal source roots are not a collision", "[bundle][collision]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, silent);

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
    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, silent);

    builder.add_reference(mesh_ref("package://ur5/meshes/a.stl", "/ws1/ur5/meshes/a.stl"));
    builder.add_reference(mesh_ref("package://ur5/meshes/b.stl", "/ws2/ur5/meshes/b.stl"));

    REQUIRE(builder.manifest().entries.size() == 1);
    REQUIRE(builder.status() == meios::emit_status::package_collision);
}

std::string second_dir(bool opt_in)
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::manifest_builder builder("botbundle", meios::collision_options{ opt_in }, sources, silent);
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

TEST_CASE("the closure re-resolves a scanner-discovered ref into the manifest", "[bundle][closure]")
{
    fake_source src;
    src.table["ur5/materials/wood.mtl"] = "/abs/ur5/materials/wood.mtl";
    meios::source_stack sources(std::move(src));
    meios::log_sink silent;
    meios::scanner_registry registry;
    registry.register_scanner("obj", meios::scanner_handle{ obj_scanner{ { "package://ur5/materials/wood.mtl" } } });

    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, silent);
    builder.add_reference(mesh_ref("package://ur5/meshes/base.obj", "/abs/ur5/meshes/base.obj"));
    builder.close_over(registry);

    const meios::asset_manifest &manifest = builder.manifest();
    REQUIRE(manifest.entries.size() == 2);
    REQUIRE(manifest.entries[1].dest_relative == "meshes/ur5/materials/wood.mtl");
}

TEST_CASE("a self-referencing scanner terminates via the visited guard", "[bundle][closure]")
{
    fake_source src;
    src.table["ur5/self.obj"] = "/abs/ur5/self.obj";
    meios::source_stack sources(std::move(src));
    meios::log_sink silent;
    meios::scanner_registry registry;
    registry.register_scanner("obj", meios::scanner_handle{ obj_scanner{ { "package://ur5/self.obj" } } });

    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, silent);
    builder.add_reference(mesh_ref("package://ur5/self.obj", "/abs/ur5/self.obj"));
    builder.close_over(registry);

    REQUIRE(builder.manifest().entries.size() == 1);
}

TEST_CASE("an unregistered extension contributes only itself", "[bundle][closure]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::scanner_registry registry;

    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, silent);
    builder.add_reference(mesh_ref("package://ur5/meshes/base.stl", "/abs/ur5/meshes/base.stl"));
    builder.close_over(registry);

    REQUIRE(builder.manifest().entries.size() == 1);
}

TEST_CASE("a texture is resolved bundle-side under the textures root", "[bundle][texture]")
{
    fake_source src;
    src.table["ur5/materials/wood.png"] = "/abs/ur5/materials/wood.png";
    meios::source_stack sources(std::move(src));
    meios::log_sink silent;

    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, silent);
    builder.add_reference(meios::reference_record{ "package://ur5/materials/wood.png", std::nullopt, true });

    const meios::asset_manifest &manifest = builder.manifest();
    REQUIRE(manifest.entries.size() == 1);
    REQUIRE(manifest.entries[0].dest_relative == "textures/ur5/materials/wood.png");
    REQUIRE(manifest.entries[0].rewritten_uri
            == "package://botbundle/textures/ur5/materials/wood.png");
}

TEST_CASE("an unresolvable texture is recorded, never dropped", "[bundle][texture]")
{
    meios::source_stack sources(fake_source{});
    meios::log_sink silent;

    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, silent);
    builder.add_reference(meios::reference_record{ "package://ghost/x.png", std::nullopt, true });

    REQUIRE(builder.manifest().entries.empty());
    REQUIRE(builder.manifest().unresolved == std::vector<std::string>{ "package://ghost/x.png" });
}

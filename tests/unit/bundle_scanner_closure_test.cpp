#include "asset_read_probe.h"
#include "bundle_scanner_fixture.h"

#include <meios/bundle.h>
#include <meios/io.h>
#include <meios/model.h>

#include <meios/bundle/scanner_handle.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

// A path-backed fake source: it answers locate for the "pkg/rel" keys it was seeded
// with, so the closure and the texture path can be driven without touching disk.
struct fake_source
{
    std::map<std::string, std::filesystem::path> table;

    meios::capability_descriptor capabilities() const
    {
        return meios::capability_descriptor{meios::source_kind::directory, false};
    }

    std::optional<meios::resolved_asset> locate(std::string_view pkg, std::string_view rel)
    {
        const std::map<std::string, std::filesystem::path>::iterator hit = table.find(std::string(pkg) + '/' + std::string(rel));
        if(hit == table.end())
            return std::nullopt;
        return meios::resolved_asset{hit->second};
    }
};

// The closure applies the regular-file rule to a discovered child, so a seeded answer must
// name a file that exists rather than a spelling nothing stands behind.
struct seeded_tree
{
    seeded_tree()
            : root(asset_probe::fresh_dir())
    {
    }

    std::filesystem::path seed(const std::string &relative) const
    {
        const std::filesystem::path file = root.path() / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary);
        return file;
    }

    meios::scratch_dir root;
};

}

static_assert(meios::package_source<fake_source>);

TEST_CASE("the closure re-resolves a scanner-discovered ref into the manifest", "[bundle][closure]")
{
    const seeded_tree tree;
    fake_source src;
    src.table["ur5/materials/wood.mtl"] = tree.seed("ur5/materials/wood.mtl");
    meios::source_stack sources(std::move(src));
    meios::log_sink silent;
    meios::scanner_registry registry;
    registry.register_scanner("obj", meios::scanner_handle{obj_scanner{{"package://ur5/materials/wood.mtl"}}});

    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);
    builder.add_reference(mesh_ref("package://ur5/meshes/base.obj", tree.seed("ur5/meshes/base.obj").string()));
    builder.close_over(registry);

    const meios::asset_manifest &manifest = builder.manifest();
    REQUIRE(manifest.entries.size() == 2);
    REQUIRE(manifest.entries[1].dest_relative == "meshes/ur5/materials/wood.mtl");
}

TEST_CASE("the closure re-anchors a bare relative scanner ref to the entry's package", "[bundle][closure]")
{
    const seeded_tree tree;
    fake_source src;
    src.table["ur5/meshes/material.mtl"] = tree.seed("ur5/meshes/material.mtl");
    meios::source_stack sources(std::move(src));
    meios::log_sink silent;
    meios::scanner_registry registry;
    registry.register_scanner("obj", meios::scanner_handle{obj_scanner{{"material.mtl"}}});

    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);
    builder.add_reference(mesh_ref("package://ur5/meshes/base.obj", tree.seed("ur5/meshes/base.obj").string()));
    builder.close_over(registry);

    const meios::asset_manifest &manifest = builder.manifest();
    REQUIRE(manifest.entries.size() == 2);
    REQUIRE(manifest.entries[1].dest_relative == "meshes/ur5/meshes/material.mtl");
    REQUIRE(manifest.unresolved.empty());
}

TEST_CASE("a scanner-discovered texture lands under the textures root", "[bundle][closure]")
{
    const seeded_tree tree;
    fake_source src;
    src.table["ur5/materials/wood.png"] = tree.seed("ur5/materials/wood.png");
    meios::source_stack sources(std::move(src));
    meios::log_sink silent;
    meios::scanner_registry registry;
    registry.register_scanner("mtl", meios::scanner_handle{obj_scanner{{"package://ur5/materials/wood.png"}}});

    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);
    builder.add_reference(mesh_ref("package://ur5/materials/wood.mtl", tree.seed("ur5/materials/wood.mtl").string()));
    builder.close_over(registry);

    const meios::asset_manifest &manifest = builder.manifest();
    REQUIRE(manifest.entries.size() == 2);
    REQUIRE(manifest.entries[1].dest_relative == "textures/ur5/materials/wood.png");
}

TEST_CASE("a self-referencing scanner terminates via the visited guard", "[bundle][closure]")
{
    const seeded_tree tree;
    const std::filesystem::path self = tree.seed("ur5/self.obj");
    fake_source src;
    src.table["ur5/self.obj"] = self;
    meios::source_stack sources(std::move(src));
    meios::log_sink silent;
    meios::scanner_registry registry;
    registry.register_scanner("obj", meios::scanner_handle{obj_scanner{{"package://ur5/self.obj"}}});

    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);
    builder.add_reference(mesh_ref("package://ur5/self.obj", self.string()));
    builder.close_over(registry);

    REQUIRE(builder.manifest().entries.size() == 1);
}

TEST_CASE("an unregistered extension contributes only itself", "[bundle][closure]")
{
    meios::source_stack sources;
    meios::log_sink silent;
    meios::scanner_registry registry;

    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);
    builder.add_reference(mesh_ref("package://ur5/meshes/base.stl", "/abs/ur5/meshes/base.stl"));
    builder.close_over(registry);

    REQUIRE(builder.manifest().entries.size() == 1);
}

TEST_CASE("a texture is resolved bundle-side under the textures root", "[bundle][texture]")
{
    const seeded_tree tree;
    fake_source src;
    src.table["ur5/materials/wood.png"] = tree.seed("ur5/materials/wood.png");
    meios::source_stack sources(std::move(src));
    meios::log_sink silent;
    meios::scanner_registry registry;
    registry.register_scanner("mtl", meios::scanner_handle{obj_scanner{{"wood.png"}}});

    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);
    builder.add_reference(mesh_ref("package://ur5/materials/base.mtl", tree.seed("ur5/materials/base.mtl").string()));
    builder.close_over(registry);

    const meios::asset_manifest &manifest = builder.manifest();
    REQUIRE(manifest.entries.size() == 2);
    REQUIRE(manifest.entries[1].dest_relative == "textures/ur5/materials/wood.png");
    REQUIRE(manifest.entries[1].rewritten_uri == "package://botbundle/textures/ur5/materials/wood.png");
}

TEST_CASE("an unresolvable texture is recorded, never dropped", "[bundle][texture]")
{
    meios::source_stack sources(fake_source{});
    meios::log_sink silent;

    meios::manifest_builder builder("botbundle", meios::collision_options{false}, sources, silent);
    builder.add_reference(meios::reference_record{"package://ghost/x.png", std::nullopt, true});

    REQUIRE(builder.manifest().entries.empty());
    REQUIRE(builder.manifest().unresolved == std::vector<std::string>{"package://ghost/x.png"});
}

#include "asset_read_probe.h"
#include "bundle_closure_probe.h"

#include <meios/bundle.h>
#include <meios/bundle/asset_bytes.h>

#include <meios/io/text_reader.h>
#include <meios/io/memory_source.h>
#include <meios/io/resolved_asset.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

TEST_CASE("a directory handed to the asset reader is refused rather than fatal", "[bundle][assets]")
{
    const meios::scratch_dir tree = asset_probe::fresh_dir();
    asset_probe::read_probe probe;

    const meios::text_read_result text = meios::read_asset_text(meios::resolved_asset{ tree.path() }, probe.log);

    REQUIRE_FALSE(text.has_value());
    REQUIRE(text.error().kind == meios::text_read_failure_kind::non_regular);
    REQUIRE(text.error().cause.operation == meios::operation_kind::status);
}

TEST_CASE("a refused directory reports exactly one coded cause-carrying record", "[bundle][assets]")
{
    const meios::scratch_dir tree = asset_probe::fresh_dir();
    asset_probe::read_probe probe;

    REQUIRE_FALSE(meios::read_asset_text(meios::resolved_asset{ tree.path() }, probe.log).has_value());

    REQUIRE(probe.records.size() == 1);
    REQUIRE(probe.records.front().lvl == meios::level::error);
    REQUIRE(probe.records.front().code == meios::diagnostic_code::cannot_open);
    REQUIRE(probe.records.front().cause.has_value());
    REQUIRE(probe.records.front().cause->operation == meios::operation_kind::status);
}

TEST_CASE("a path that does not exist carries its own classification and native cause", "[bundle][assets]")
{
    const meios::scratch_dir tree = asset_probe::fresh_dir();
    asset_probe::read_probe probe;

    const meios::text_read_result text = meios::read_asset_text(meios::resolved_asset{ tree.path() / "absent.obj" }, probe.log);

    REQUIRE_FALSE(text.has_value());
    REQUIRE(text.error().kind == meios::text_read_failure_kind::open);
    REQUIRE(text.error().cause.operation == meios::operation_kind::open);
    REQUIRE(text.error().cause.native.value() != 0);
    REQUIRE(probe.records.size() == 1);
    REQUIRE(probe.records.front().code == meios::diagnostic_code::cannot_open);
}

TEST_CASE("a regular file is read verbatim and reports nothing", "[bundle][assets]")
{
    const meios::scratch_dir tree = asset_probe::fresh_dir();
    asset_probe::write_file(tree.path() / "mesh.obj", "mtllib wood.mtl\n");
    asset_probe::read_probe probe;

    const meios::text_read_result text = meios::read_asset_text(meios::resolved_asset{ tree.path() / "mesh.obj" }, probe.log);

    REQUIRE(text.has_value());
    REQUIRE(*text == "mtllib wood.mtl\n");
    REQUIRE(probe.records.empty());
}

// An empty regular file is the control the scanner claims rest on: it reads as a value, not as a
// failure, so a scanner branching on the error arm still reaches its parser for one.
TEST_CASE("an empty regular file reads as an empty value, never as a failure", "[bundle][assets]")
{
    const meios::scratch_dir tree = asset_probe::fresh_dir();
    asset_probe::write_file(tree.path() / "mesh.obj", "");
    asset_probe::read_probe probe;

    const meios::text_read_result text = meios::read_asset_text(meios::resolved_asset{ tree.path() / "mesh.obj" }, probe.log);

    REQUIRE(text.has_value());
    REQUIRE(text->empty());
    REQUIRE(probe.records.empty());
}

TEST_CASE("an asset carrying a source root is read through the root-relative reopen", "[bundle][assets]")
{
    asset_probe::read_probe probe;
    meios::memory_source source{ probe.log };
    source.add("pkg", "meshes/base.obj", "mtllib base.mtl\n");
    const std::optional<meios::resolved_asset> hit = source.locate("pkg", "meshes/base.obj");
    REQUIRE(hit.has_value());
    REQUIRE(hit->source_root().has_value());
    REQUIRE(hit->source_relative().has_value());

    const meios::text_read_result text = meios::read_asset_text(*hit, probe.log);

    REQUIRE(text.has_value());
    REQUIRE(*text == "mtllib base.mtl\n");
    REQUIRE(probe.records.empty());
}

static_assert(meios::package_source<asset_probe::counting_source>);
static_assert(meios::provides_typed_lookup<asset_probe::counting_source>);
static_assert(meios::asset_scanner<asset_probe::child_scanner>);

TEST_CASE("a reference the resolver refused is never asked about again", "[bundle][manifest]")
{
    int calls = 0;
    meios::source_stack sources(asset_probe::counting_source{ { { "ur5/materials/wood.png", "/abs/ur5/materials/wood.png" } }, calls });
    asset_probe::read_probe probe;
    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, probe.log);

    builder.add_reference(meios::reference_record{ "package://ur5/materials/wood.png", std::nullopt, true });

    REQUIRE(builder.manifest().entries.empty());
    REQUIRE(builder.manifest().unresolved == std::vector<std::string>{ "package://ur5/materials/wood.png" });
    REQUIRE(calls == 0);
}

TEST_CASE("a populated resolved path is used as given and re-resolved by nothing", "[bundle][manifest]")
{
    int calls = 0;
    meios::source_stack sources(asset_probe::counting_source{ { { "ur5/meshes/base.stl", "/abs/ur5/meshes/base.stl" } }, calls });
    asset_probe::read_probe probe;
    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, probe.log);

    builder.add_reference(meios::reference_record{ "package://ur5/meshes/base.stl", std::string{ "/abs/ur5/meshes/base.stl" }, false });

    REQUIRE(builder.manifest().entries.size() == 1);
    REQUIRE(builder.manifest().entries[0].dest_relative == "meshes/ur5/meshes/base.stl");
    REQUIRE(calls == 0);
}

TEST_CASE("a discovered child is resolved exactly once, at discovery", "[bundle][closure]")
{
    const meios::scratch_dir tree = asset_probe::fresh_dir();
    const asset_probe::package_tree pkg{ tree.path() };
    asset_probe::write_file(pkg.child, "");
    int calls = 0;
    int dispatches = 0;
    meios::source_stack sources(asset_probe::counting_source{ { { "ur5/materials/wood.mtl", pkg.child } }, calls });
    asset_probe::read_probe probe;
    meios::scanner_registry registry = asset_probe::closure_registry(dispatches);

    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, probe.log);
    builder.add_reference(meios::reference_record{ "package://ur5/meshes/base.obj", pkg.parent.string(), false });
    builder.close_over(registry);

    REQUIRE(builder.manifest().entries.size() == 2);
    REQUIRE(builder.manifest().entries[1].dest_relative == "meshes/ur5/materials/wood.mtl");
    REQUIRE(calls == 1);
    REQUIRE(dispatches == 2);
}

TEST_CASE("a child the source answers with a directory reaches neither the manifest nor a scanner", "[bundle][closure]")
{
    const meios::scratch_dir tree = asset_probe::fresh_dir();
    const asset_probe::package_tree pkg{ tree.path() };
    std::filesystem::create_directories(pkg.child);
    int calls = 0;
    int dispatches = 0;
    meios::source_stack sources(asset_probe::counting_source{ { { "ur5/materials/wood.mtl", pkg.child } }, calls });
    asset_probe::read_probe probe;
    meios::scanner_registry registry = asset_probe::closure_registry(dispatches);

    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, probe.log);
    builder.add_reference(meios::reference_record{ "package://ur5/meshes/base.obj", pkg.parent.string(), false });
    builder.close_over(registry);

    REQUIRE(builder.manifest().entries.size() == 1);
    REQUIRE(builder.manifest().unresolved == std::vector<std::string>{ "package://ur5/materials/wood.mtl" });
    REQUIRE(calls == 1);
    REQUIRE(dispatches == 1);
}

TEST_CASE("a child the source cannot answer is reported once and left unresolved", "[bundle][closure]")
{
    int calls = 0;
    int dispatches = 0;
    meios::source_stack sources(asset_probe::counting_source{ {}, calls });
    asset_probe::read_probe probe;
    meios::scanner_registry registry = asset_probe::closure_registry(dispatches);

    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, probe.log);
    builder.add_reference(meios::reference_record{ "package://ur5/meshes/base.obj", std::string{ "/abs/ur5/meshes/base.obj" }, false });
    builder.close_over(registry);

    REQUIRE(builder.manifest().entries.size() == 1);
    REQUIRE(builder.manifest().unresolved == std::vector<std::string>{ "package://ur5/materials/wood.mtl" });
    REQUIRE(probe.records.size() == 1);
    REQUIRE(probe.records.front().code == meios::diagnostic_code::unresolved_asset);
    REQUIRE(calls == 1);
}

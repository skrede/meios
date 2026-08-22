#include "asset_read_probe.h"
#include "bundle_closure_probe.h"

#include <meios/bundle.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>
#include <system_error>

static_assert(meios::package_source<asset_probe::counting_source>);
static_assert(meios::provides_typed_lookup<asset_probe::counting_source>);
static_assert(meios::asset_scanner<asset_probe::child_scanner>);

TEST_CASE("a child two parents discover is resolved exactly once, at discovery", "[bundle][closure]")
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
    builder.add_reference(meios::reference_record{ "package://ur5/meshes/tool.obj", pkg.sibling.string(), false });
    builder.close_over(registry);

    REQUIRE(builder.manifest().entries.size() == 3);
    REQUIRE(builder.manifest().entries[2].dest_relative == "meshes/ur5/materials/wood.mtl");
    REQUIRE(calls == 1);
    REQUIRE(dispatches == 3);
}

TEST_CASE("a child two parents cannot resolve is reported exactly once", "[bundle][closure]")
{
    int calls = 0;
    int dispatches = 0;
    meios::source_stack sources(asset_probe::counting_source{ {}, calls });
    asset_probe::read_probe probe;
    meios::scanner_registry registry = asset_probe::closure_registry(dispatches);

    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, probe.log);
    builder.add_reference(meios::reference_record{ "package://ur5/meshes/base.obj", std::string{ "/abs/ur5/meshes/base.obj" }, false });
    builder.add_reference(meios::reference_record{ "package://ur5/meshes/tool.obj", std::string{ "/abs/ur5/meshes/tool.obj" }, false });
    builder.close_over(registry);

    REQUIRE(builder.manifest().entries.size() == 2);
    REQUIRE(builder.manifest().unresolved == std::vector<std::string>{ "package://ur5/materials/wood.mtl" });
    REQUIRE(probe.records.size() == 1);
    REQUIRE(probe.records.front().code == meios::diagnostic_code::unresolved_asset);
    REQUIRE(calls == 1);
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

// An over-long component is the one determined status failure reachable without privileges, but a
// standard library is free to report it as plain absence; probing first keeps that host honest.
TEST_CASE("a child whose status cannot be determined is refused with its native cause", "[bundle][closure]")
{
    const std::filesystem::path undeterminable = std::filesystem::temp_directory_path() / std::string(400, 'n') / "wood.mtl";
    std::error_code probing;
    static_cast<void>(std::filesystem::status(undeterminable, probing));
    if(!probing)
        SKIP("this host reports an over-long path as absence, leaving no determined status failure to drive");

    const meios::scratch_dir tree = asset_probe::fresh_dir();
    const asset_probe::package_tree pkg{ tree.path() };
    int calls = 0;
    int dispatches = 0;
    meios::source_stack sources(asset_probe::counting_source{ { { "ur5/materials/wood.mtl", undeterminable } }, calls });
    asset_probe::read_probe probe;
    meios::scanner_registry registry = asset_probe::closure_registry(dispatches);

    meios::manifest_builder builder("botbundle", meios::collision_options{ false }, sources, probe.log);
    builder.add_reference(meios::reference_record{ "package://ur5/meshes/base.obj", pkg.parent.string(), false });
    builder.close_over(registry);

    REQUIRE(builder.manifest().unresolved == std::vector<std::string>{ "package://ur5/materials/wood.mtl" });
    REQUIRE(probe.records.size() == 2);
    REQUIRE(probe.records[0].lvl == meios::level::error);
    REQUIRE(probe.records[0].code == meios::diagnostic_code::cannot_open);
    REQUIRE(probe.records[0].cause.has_value());
    REQUIRE(probe.records[0].cause->operation == meios::operation_kind::status);
    REQUIRE(probe.records[0].cause->native.value() != 0);
    REQUIRE(probe.records[1].code == meios::diagnostic_code::unresolved_asset);
}

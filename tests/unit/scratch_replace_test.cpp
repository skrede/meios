#include "scratch_source.h"

#include <meios/diagnostic/claims.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <filesystem>

namespace
{

using scratch_test::probe;
using scratch_test::refusals;
using scratch_test::read_file;
using scratch_test::entry_count;
using scratch_test::locate_path;

}

TEST_CASE("the entry-update vocabulary spells itself", "[io][scratch][replace]")
{
    REQUIRE(meios::to_string(meios::update_behavior::reject) == "reject");
    REQUIRE(meios::to_string(meios::update_behavior::replace) == "replace");
    REQUIRE(meios::to_string(meios::diagnostic_code::duplicate_asset_entry) == "duplicate_asset_entry");
    REQUIRE(meios::cleared_by(meios::diagnostic_code::duplicate_asset_entry) == meios::completeness::none);
}

TEST_CASE("a duplicate key is refused and the first bytes keep resolving", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    held.source.add("pkg", "meshes/arm.dae", "second-bytes");

    REQUIRE(refusals(held.events, meios::diagnostic_code::duplicate_asset_entry, false) == 1);
    REQUIRE(refusals(held.events, meios::diagnostic_code::asset_write_failed, false) == 0);
    REQUIRE(read_file(locate_path(held.source, "meshes/arm.dae")) == "first-bytes");
}

TEST_CASE("a source built to replace accepts the second offer", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    held.source.add("pkg", "meshes/arm.dae", "second-bytes");

    REQUIRE(refusals(held.events, meios::diagnostic_code::duplicate_asset_entry, false) == 0);
    REQUIRE(refusals(held.events, meios::diagnostic_code::asset_write_failed, false) == 0);
    REQUIRE(read_file(locate_path(held.source, "meshes/arm.dae")) == "second-bytes");
}

TEST_CASE("a replacement publishes to the path a resolution already handed out", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");

    const std::filesystem::path before = locate_path(held.source, "meshes/arm.dae");
    held.source.add("pkg", "meshes/arm.dae", "second-bytes");
    const std::filesystem::path after = locate_path(held.source, "meshes/arm.dae");

    REQUIRE(after == before);
    REQUIRE(read_file(before) == "second-bytes");
}

TEST_CASE("a replaced entry leaves one regular file and no generation directory", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    const std::filesystem::path located = locate_path(held.source, "meshes/arm.dae");
    held.source.add("pkg", "meshes/arm.dae", "second-bytes");

    const std::filesystem::path dir = located.parent_path();
    REQUIRE(entry_count(dir) == 1);
    REQUIRE(std::filesystem::is_regular_file(dir / "arm.dae"));
}

TEST_CASE("a sibling still resolves by a plain relative join after a replacement", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "collada-bytes");
    held.source.add("pkg", "meshes/arm.mtl", "material-bytes");

    const std::filesystem::path model   = locate_path(held.source, "meshes/arm.dae");
    const std::filesystem::path sibling = locate_path(held.source, "meshes/arm.mtl");
    held.source.add("pkg", "meshes/arm.dae", "replaced-bytes");

    REQUIRE(sibling == model.parent_path() / "arm.mtl");
    REQUIRE(read_file(model) == "replaced-bytes");
    REQUIRE(read_file(sibling) == "material-bytes");
    REQUIRE(entry_count(model.parent_path()) == 2);
    for(const std::filesystem::directory_entry &child : std::filesystem::directory_iterator(model.parent_path()))
        REQUIRE(child.is_regular_file());
}

TEST_CASE("an entry deleted outside the source is rematerialized at the same path", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");

    const std::filesystem::path before = locate_path(held.source, "meshes/arm.dae");
    std::filesystem::remove(before);
    REQUIRE_FALSE(std::filesystem::exists(before));
    const std::filesystem::path after = locate_path(held.source, "meshes/arm.dae");

    REQUIRE(after == before);
    REQUIRE(read_file(after) == "first-bytes");
}

namespace
{

void expect_own_file_after_replacement(probe &held, const std::filesystem::path &model, const std::filesystem::path &sibling)
{
    held.source.add("pkg", "meshes/arm.dae", "replaced-bytes");

    REQUIRE(model != sibling);
    REQUIRE(std::filesystem::is_regular_file(model));
    REQUIRE(std::filesystem::is_regular_file(sibling));
    REQUIRE(read_file(model) == "replaced-bytes");
    REQUIRE(read_file(sibling) == "sibling-bytes");
    REQUIRE(entry_count(model.parent_path()) == 2);
}

}

TEST_CASE("an entry whose relative extends another entry's keeps its own file through a replacement", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    held.source.add("pkg", "meshes/arm.dae.meios-incoming", "sibling-bytes");

    const std::filesystem::path model   = locate_path(held.source, "meshes/arm.dae");
    const std::filesystem::path sibling = locate_path(held.source, "meshes/arm.dae.meios-incoming");

    expect_own_file_after_replacement(held, model, sibling);
}

TEST_CASE("the extending pair keeps its files with the offer and resolution order reversed", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae.meios-incoming", "sibling-bytes");
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");

    const std::filesystem::path sibling = locate_path(held.source, "meshes/arm.dae.meios-incoming");
    const std::filesystem::path model   = locate_path(held.source, "meshes/arm.dae");

    expect_own_file_after_replacement(held, model, sibling);
}

TEST_CASE("an entry offered into the publication staging directory is refused", "[io][scratch][replace]")
{
    REQUIRE(meios::detail::scratch_staging_dir == ".meios-staging");
    const std::string staging{meios::detail::scratch_staging_dir};
    const std::string traversing = "../" + staging + '/' + std::string{meios::detail::scratch_staging_file};
    const std::string neighbor   = "meshes/" + staging + "-notes";

    probe held{meios::update_behavior::reject};
    held.source.add(staging, std::string{meios::detail::scratch_staging_file}, "direct-bytes");
    held.source.add("pkg", traversing, "traversing-bytes");
    held.source.add("pkg", neighbor, "notes-bytes");

    REQUIRE(refusals(held.events, meios::diagnostic_code::malformed_asset_uri, false) == 2);
    REQUIRE_FALSE(held.source.locate(staging, meios::detail::scratch_staging_file).has_value());
    REQUIRE_FALSE(held.source.locate("pkg", traversing).has_value());
    REQUIRE(read_file(locate_path(held.source, neighbor.c_str())) == "notes-bytes");
}

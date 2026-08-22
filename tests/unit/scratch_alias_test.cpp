#include "scratch_source.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <filesystem>

namespace
{

using scratch_test::probe;
using scratch_test::refusals;
using scratch_test::read_file;
using scratch_test::entry_count;
using scratch_test::locate_path;

struct spelling
{
    const char *package;
    const char *relative;
};

std::filesystem::path scratch_root(meios::memory_source &source, const char *relative)
{
    const std::optional<meios::resolved_asset> hit = source.locate("pkg", relative);
    REQUIRE(hit.has_value());
    REQUIRE(hit->source_root().has_value());
    return *hit->source_root();
}

std::optional<meios::operation_failure> failure_of(const scratch_test::event_log &events, meios::diagnostic_code code)
{
    for(const scratch_test::event &recorded : events)
        if(recorded.code == code && recorded.cause)
            return recorded.cause;
    return std::nullopt;
}

// Both orders resolve through the plain spelling because that is the one entry either order
// leaves behind, whichever of the two was offered first.
void expect_collision(const spelling &first, const spelling &second)
{
    INFO("offered " << first.package << '/' << first.relative << " then " << second.package << '/' << second.relative);
    probe held{meios::update_behavior::reject};
    held.source.add(first.package, first.relative, "first-bytes");
    held.source.add(second.package, second.relative, "second-bytes");

    CHECK(refusals(held.events, meios::diagnostic_code::duplicate_asset_entry, false) == 1);
    CHECK(refusals(held.events, meios::diagnostic_code::asset_write_failed, true) == 0);
    CHECK(read_file(locate_path(held.source, "meshes/arm.dae")) == "first-bytes");
}

}

TEST_CASE("a second spelling of one mirrored path is refused as a duplicate", "[io][scratch][alias]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    held.source.add("pkg", "meshes/./arm.dae", "second-bytes");

    REQUIRE(refusals(held.events, meios::diagnostic_code::duplicate_asset_entry, false) == 1);
    REQUIRE(refusals(held.events, meios::diagnostic_code::asset_write_failed, true) == 0);

    const std::filesystem::path plain = locate_path(held.source, "meshes/arm.dae");
    REQUIRE(read_file(plain) == "first-bytes");
    REQUIRE(locate_path(held.source, "meshes/./arm.dae") == plain);
    REQUIRE(read_file(plain) == "first-bytes");
    REQUIRE(entry_count(plain.parent_path()) == 1);
}

TEST_CASE("a resolution reports the spelling it was asked for", "[io][scratch][alias]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");

    const std::optional<meios::resolved_asset> hit = held.source.locate("pkg", "meshes/./arm.dae");
    REQUIRE(hit.has_value());
    REQUIRE(hit->source_relative() == std::filesystem::path("pkg") / "meshes/./arm.dae");
    REQUIRE(read_file(hit->path()) == "first-bytes");
}

TEST_CASE("an offer that names no file beneath its package is refused rather than looped over", "[io][scratch][alias]")
{
    probe climbing{meios::update_behavior::reject};
    climbing.source.add("pkg", "..", "climb-bytes");
    REQUIRE_FALSE(climbing.source.locate("pkg", "..").has_value());

    REQUIRE(refusals(climbing.events, meios::diagnostic_code::malformed_asset_uri, false) == 1);
    REQUIRE(refusals(climbing.events, meios::diagnostic_code::duplicate_asset_entry, false) == 0);

    probe standing{meios::update_behavior::reject};
    standing.source.add("", ".", "dot-bytes");
    REQUIRE_FALSE(standing.source.locate("", ".").has_value());

    REQUIRE(refusals(standing.events, meios::diagnostic_code::malformed_asset_uri, false) == 1);
    REQUIRE(refusals(standing.events, meios::diagnostic_code::duplicate_asset_entry, false) == 0);
}

// A relative half carrying a root replaces the package half outright rather than joining under
// it, and the composed path it leaves is its own parent — so the fold has no component to drop
// and the offer names no file the source could serve either way.
TEST_CASE("an offer whose relative half carries a root is refused rather than looped over", "[io][scratch][alias]")
{
    const char *rooted[] = { "/", "//", "/foo/..", "/meshes/arm.dae" };

    for(const char *relative : rooted)
    {
        INFO("offered pkg/" << relative);
        probe held{meios::update_behavior::reject};
        held.source.add("pkg", relative, "rooted-bytes");

        CHECK(refusals(held.events, meios::diagnostic_code::malformed_asset_uri, false) == 1);
        CHECK_FALSE(held.source.locate("pkg", relative).has_value());
        CHECK(refusals(held.events, meios::diagnostic_code::duplicate_asset_entry, false) == 0);
    }
}

// The aliasing spellings above collide at the offer and are never resolved through; this one
// resolves through the alias first, which is where a path derived from the authored text rather
// than from the folded key puts a directory in the entry's place.
TEST_CASE("an entry resolved through a trailing separator materializes its file", "[io][scratch][alias]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");

    const std::filesystem::path trailing = locate_path(held.source, "meshes/arm.dae/");
    REQUIRE(std::filesystem::is_regular_file(trailing));
    REQUIRE(read_file(trailing) == "first-bytes");
    REQUIRE(locate_path(held.source, "meshes/arm.dae") == trailing);
    REQUIRE(read_file(trailing) == "first-bytes");
    REQUIRE(entry_count(trailing.parent_path()) == 1);
    REQUIRE(refusals(held.events, meios::diagnostic_code::asset_write_failed, true) == 0);
}

TEST_CASE("a relative naming the package directory is not a duplicate of the package itself", "[io][scratch][alias]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("", "pkg", "package-bytes");
    held.source.add("pkg", ".", "dot-bytes");

    REQUIRE(refusals(held.events, meios::diagnostic_code::malformed_asset_uri, false) == 1);
    REQUIRE(refusals(held.events, meios::diagnostic_code::duplicate_asset_entry, false) == 0);
    REQUIRE(read_file(held.source.locate("", "pkg")->path()) == "package-bytes");
}

TEST_CASE("an offer that climbs out of its package is refused and writes nothing beside it", "[io][scratch][alias]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");

    const std::filesystem::path root = scratch_root(held.source, "meshes/arm.dae");
    const std::size_t before         = entry_count(root);
    held.source.add("pkg", "../other/x.dae", "escaped-bytes");

    REQUIRE(refusals(held.events, meios::diagnostic_code::malformed_asset_uri, false) == 1);
    REQUIRE_FALSE(held.source.locate("pkg", "../other/x.dae").has_value());
    REQUIRE_FALSE(std::filesystem::exists(root / "other"));
    REQUIRE(entry_count(root) == before);
}

TEST_CASE("four alias forms collide with the plain spelling in both offer orders", "[io][scratch][alias]")
{
    const spelling plain{ "pkg", "meshes/arm.dae" };
    const spelling aliases[] = {
        { "pkg", "meshes/./arm.dae" },
        { "pkg", "meshes/x/../arm.dae" },
        { "pkg/meshes", "arm.dae" },
        { "pkg", "meshes/arm.dae/" },
    };

    for(const spelling &alias : aliases)
    {
        expect_collision(plain, alias);
        expect_collision(alias, plain);
    }
}

TEST_CASE("a source built to replace accepts an aliasing offer through the one entry it holds", "[io][scratch][alias]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    const std::filesystem::path plain = locate_path(held.source, "meshes/arm.dae");
    held.source.add("pkg", "meshes/./arm.dae", "second-bytes");

    REQUIRE(refusals(held.events, meios::diagnostic_code::duplicate_asset_entry, false) == 0);
    REQUIRE(refusals(held.events, meios::diagnostic_code::asset_write_failed, true) == 0);
    REQUIRE(locate_path(held.source, "meshes/./arm.dae") == plain);
    REQUIRE(read_file(plain) == "second-bytes");
    REQUIRE(entry_count(plain.parent_path()) == 1);
}

TEST_CASE("two entries that name different files each keep their own", "[io][scratch][alias]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "collada-bytes");
    held.source.add("pkg", "meshes/arm.mtl", "material-bytes");

    const std::filesystem::path model   = locate_path(held.source, "meshes/arm.dae");
    const std::filesystem::path sibling = locate_path(held.source, "meshes/arm.mtl");

    REQUIRE(model != sibling);
    REQUIRE(read_file(model) == "collada-bytes");
    REQUIRE(read_file(sibling) == "material-bytes");
    REQUIRE(entry_count(model.parent_path()) == 2);
    REQUIRE(refusals(held.events, meios::diagnostic_code::duplicate_asset_entry, false) == 0);
    REQUIRE(refusals(held.events, meios::diagnostic_code::asset_write_failed, true) == 0);
}

// A directory symlink under the scratch root gives two keys one canonical file with no dot
// segment anywhere, which is the shape a case-folding volume produces and this host cannot.
#ifndef _WIN32
TEST_CASE("a publication onto a file another entry already holds is refused", "[io][scratch][alias]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    const std::filesystem::path model = locate_path(held.source, "meshes/arm.dae");
    std::filesystem::create_directory_symlink(model.parent_path(), scratch_root(held.source, "meshes/arm.dae") / "link");

    held.source.add("link", "arm.dae", "aliased-bytes");

    REQUIRE_FALSE(held.source.locate("link", "arm.dae").has_value());
    REQUIRE(refusals(held.events, meios::diagnostic_code::asset_write_failed, true) == 1);
    const std::optional<meios::operation_failure> cause = failure_of(held.events, meios::diagnostic_code::asset_write_failed);
    REQUIRE(cause.has_value());
    REQUIRE(cause->operation == meios::operation_kind::publish);
    REQUIRE(read_file(model) == "first-bytes");
}

// The file going away between the two publications is the state rematerialization exists for, and
// it is the state the equivalence guard cannot measure: absence is reported as "not equivalent".
TEST_CASE("a publication onto a vanished entry's file is refused rather than silently taken", "[io][scratch][alias]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    const std::filesystem::path model = locate_path(held.source, "meshes/arm.dae");
    std::filesystem::create_directory_symlink(model.parent_path(), scratch_root(held.source, "meshes/arm.dae") / "link");
    std::filesystem::remove(model);

    held.source.add("link", "arm.dae", "aliased-bytes");

    REQUIRE_FALSE(held.source.locate("link", "arm.dae").has_value());
    REQUIRE(refusals(held.events, meios::diagnostic_code::asset_write_failed, true) == 1);
    REQUIRE(read_file(locate_path(held.source, "meshes/arm.dae")) == "first-bytes");
}
#endif

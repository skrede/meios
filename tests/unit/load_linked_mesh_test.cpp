#include "xacro_include_tree.h"
#include "linked_install_tree.h"

#include <meios/urdf.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>
#include <algorithm>
#include <filesystem>
#include <system_error>

namespace
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

std::string stl()
{
    return "solid base\nendsolid base\n";
}

std::string mesh_robot(const std::string &filename)
{
    return "<robot name=\"probe\"><link name=\"base\"><visual><geometry><mesh filename=\"" + filename + "\"/></geometry></visual></link></robot>";
}

linked_install::files mesh_sources()
{
    return {{"robot.urdf", mesh_robot("base.stl")}, {"base.stl", stl()}};
}

meios::load_options following()
{
    meios::load_options opts;
    opts.includes.follow_outside_links = true;
    return opts;
}

bool resolves_to(const load_outcome &loaded, const std::filesystem::path &expected)
{
    if(!loaded.has_value() || loaded->robot.links.size() != 1 || loaded->robot.links[0].visuals.size() != 1)
        return false;
    const auto *mesh = std::get_if<meios::mesh<>>(&loaded->robot.links[0].visuals[0].geom.shape);
    return mesh != nullptr && mesh->resolved_path && std::filesystem::equivalent(*mesh->resolved_path, expected);
}

bool unresolved_through_non_directory(const meios::captured_diagnostic &record)
{
    return record.code == meios::diagnostic_code::unresolved_asset && record.cause && record.cause->native == std::errc::not_a_directory;
}

}

TEST_CASE("a relative mesh resolves beside a document opened through per-file install links", "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_install::per_file_links(held, mesh_sources(), "robot.urdf");
    if(document.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome loaded = meios::load(document);
    if(!loaded)
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
    REQUIRE(resolves_to(loaded, held.root() / "src" / "urdf" / "base.stl"));
}

TEST_CASE("a mesh linked into neither directory of its document resolves with outside links followed", "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_install::per_file_links(held, {{"robot.urdf", mesh_robot("base.stl")}}, "robot.urdf");
    const std::filesystem::path target   = held.write("build/base.stl", stl());
    if(document.empty() || linked_install::link_elsewhere(held, "base.stl", target).empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome loaded = meios::load(document, following());
    if(!loaded)
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
    REQUIRE(resolves_to(loaded, target));
}

TEST_CASE("a mesh path climbing out of its document's directories stays refused with outside links followed", "[urdf][load][mesh]")
{
    const include_tree::tree held;
    const std::filesystem::path document = held.write("src/urdf/robot.urdf", mesh_robot("../outside/base.stl"));
    held.write("src/outside/base.stl", stl());

    const load_outcome refused = meios::load(document, following());
    REQUIRE_FALSE(refused.has_value());
    REQUIRE(refused.error().code == meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("a relative mesh resolves beside a document opened through a linked install directory", "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_install::linked_directory(held, mesh_sources(), "robot.urdf");
    if(document.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    REQUIRE(resolves_to(meios::load(document), held.root() / "src" / "urdf" / "base.stl"));
}

TEST_CASE("a mesh linked into neither directory of its document refuses with the default options", "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_install::per_file_links(held, {{"robot.urdf", mesh_robot("base.stl")}}, "robot.urdf");
    const std::filesystem::path target   = held.write("build/base.stl", stl());
    if(document.empty() || linked_install::link_elsewhere(held, "base.stl", target).empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome refused = meios::load(document);
    REQUIRE_FALSE(refused.has_value());
    REQUIRE(refused.error().code == meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("a dangling mesh link beside its document is a missing asset, never a root's copy, in both option states", "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    held.write("root/m.stl", stl());
    const std::filesystem::path document = held.write("doc/robot.urdf", mesh_robot("m.stl"));
    if(!include_tree::linked(held.root() / "outside" / "m.stl", held.root() / "doc" / "m.stl"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    meios::load_options unset;
    unset.package_roots          = {held.root() / "root"};
    meios::load_options followed = following();
    followed.package_roots       = unset.package_roots;

    for(const load_outcome &refused : {meios::load(document, unset), meios::load(document, followed)})
    {
        REQUIRE_FALSE(resolves_to(refused, held.root() / "root" / "m.stl"));
        REQUIRE_FALSE(refused.has_value());
        REQUIRE(refused.error().code == meios::diagnostic_code::unresolved_asset);
    }
}

TEST_CASE("a mesh spelled through a file that is not a directory does not resolve, and its record names why", "[urdf][load][mesh]")
{
#ifdef _WIN32
    SKIP("the Windows path API reduces .. before the filesystem sees the element that is not a directory");
#endif
    const include_tree::tree held;
    held.write("doc/f.txt", "plain");
    held.write("doc/m.stl", stl());
    const std::filesystem::path document = held.write("doc/robot.urdf", mesh_robot("f.txt/../m.stl"));

    const load_outcome refused = meios::load(document);
    REQUIRE_FALSE(resolves_to(refused, held.root() / "doc" / "m.stl"));
    REQUIRE_FALSE(refused.has_value());
    REQUIRE(refused.error().code == meios::diagnostic_code::unresolved_asset);
    REQUIRE(std::ranges::any_of(refused.error().diagnostics, unresolved_through_non_directory));
}

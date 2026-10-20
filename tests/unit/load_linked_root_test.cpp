#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/urdf.h>

#include <meios/io/text_reader.h>
#include <meios/io/bundle_source.h>
#include <meios/io/resolved_asset.h>
#include <meios/io/directory_source.h>

#include <meios/bundle/asset_bytes.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <filesystem>
#include <system_error>

namespace
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

std::filesystem::path linked_root(const include_tree::tree &held)
{
    std::error_code ec;
    std::filesystem::create_directory_symlink(held.root() / "real", held.root() / "rootlink", ec);
    return ec ? std::filesystem::path{} : held.root() / "rootlink";
}

meios::load_options rooted_at(const std::filesystem::path &root)
{
    meios::load_options opts;
    opts.package_roots = {root};
    return opts;
}

std::set<std::string> link_names(const load_outcome &loaded)
{
    std::set<std::string> names;
    if(!loaded)
    {
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
        return names;
    }
    for(const auto &link : loaded->robot.links)
        names.insert(link.name);
    return names;
}

std::string including_robot()
{
    const std::string joint = "<joint name=\"j\" type=\"fixed\"><parent link=\"pkg_inc\"/><child link=\"find_inc\"/></joint>";
    return include_tree::robot(include_tree::include("package://p/a.xacro") + include_tree::include("$(find p)/b.xacro") + joint);
}

std::string mesh_robot()
{
    return "<robot name=\"probe\"><link name=\"base\"><visual><geometry><mesh filename=\"package://p/meshes/m.stl\"/></geometry></visual></link></robot>";
}

bool resolves_to(const load_outcome &loaded, const std::filesystem::path &expected)
{
    if(!loaded.has_value() || loaded->robot.links.size() != 1 || loaded->robot.links[0].visuals.size() != 1)
        return false;
    const auto *mesh = std::get_if<meios::mesh<>>(&loaded->robot.links[0].visuals[0].geom.shape);
    return mesh != nullptr && mesh->resolved_path && std::filesystem::equivalent(*mesh->resolved_path, expected);
}

template<typename Source>
void reads_through_real_root(const std::filesystem::path &root, const std::string &bytes)
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    Source source{root, log};
    const std::optional<meios::resolved_asset> hit = source.locate("p", "meshes/m.stl");
    REQUIRE(hit.has_value());
    REQUIRE(hit->source_root() == std::filesystem::weakly_canonical(root));
    REQUIRE(hit->source_relative() == hit->path().lexically_relative(*hit->source_root()));

    const meios::text_read_result text = meios::read_asset_text(*hit, log);
    REQUIRE(text.has_value());
    REQUIRE(*text == bytes);
    REQUIRE(xacro_probe::errors(records) == 0);
}

}

TEST_CASE("a directory source and a bundle source over a linked root report its real directory and read the asset through it", "[io][source][link]")
{
    const include_tree::tree held;
    held.write("real/p/meshes/m.stl", "solid m\nendsolid m\n");
    const std::filesystem::path root = linked_root(held);
    if(root.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    reads_through_real_root<meios::directory_source>(root, "solid m\nendsolid m\n");
    reads_through_real_root<meios::bundle_source>(root, "solid m\nendsolid m\n");
}

TEST_CASE("a package root given as a link serves its package includes", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    held.write("real/p/a.xacro", include_tree::robot("<link name=\"pkg_inc\"/>"));
    held.write("real/p/b.xacro", include_tree::robot("<link name=\"find_inc\"/>"));
    const std::filesystem::path document = held.write("doc/robot.xacro", including_robot());
    const std::filesystem::path root     = linked_root(held);
    if(root.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    REQUIRE(link_names(meios::load(document, rooted_at(root))) == std::set<std::string>{"pkg_inc", "find_inc"});
}

TEST_CASE("a package root given as a link resolves a package mesh", "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    const std::filesystem::path mesh     = held.write("real/p/meshes/m.stl", "solid m\nendsolid m\n");
    const std::filesystem::path document = held.write("doc/robot.urdf", mesh_robot());
    const std::filesystem::path root     = linked_root(held);
    if(root.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome loaded = meios::load(document, rooted_at(root));
    if(!loaded)
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
    REQUIRE(resolves_to(loaded, mesh));
}

#ifdef MEIOS_HAS_YAML

namespace
{

std::string yaml_robot(const std::string &spec, const std::string &key)
{
    return include_tree::robot("<link name=\"${xacro.load_yaml('" + spec + "')['" + key + "']}\"/>");
}

}

TEST_CASE("a package root given as a link serves its package-form auxiliary documents", "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    held.write("real/p/cfg.yaml", "pkg: pkg_yaml\nfind: find_yaml\n");
    const std::filesystem::path by_package = held.write("doc/package.xacro", yaml_robot("package://p/cfg.yaml", "pkg"));
    const std::filesystem::path by_find    = held.write("doc/find.xacro", yaml_robot("$(find p)/cfg.yaml", "find"));
    const std::filesystem::path root       = linked_root(held);
    if(root.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    CHECK(link_names(meios::load(by_package, rooted_at(root))) == std::set<std::string>{"pkg_yaml"});
    CHECK(link_names(meios::load(by_find, rooted_at(root))) == std::set<std::string>{"find_yaml"});
}

#endif

TEST_CASE("a package reference through a root whose files link out of it refuses as uncontained", "[urdf][load][link]")
{
    const include_tree::tree held;
    const std::filesystem::path share    = held.root() / "install" / "share";
    const std::filesystem::path mesh     = held.write("src/p/meshes/m.stl", "solid m\nendsolid m\n");
    const std::filesystem::path document = held.write("doc/robot.urdf", mesh_robot());
    std::filesystem::create_directories(share / "p" / "meshes");
    if(!include_tree::linked(mesh, share / "p/meshes/m.stl") || !include_tree::linked(held.write("src/p/cfg.yaml", "pkg: pkg_yaml\n"), share / "p/cfg.yaml"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome refused = meios::load(document, rooted_at(share));
    REQUIRE((!refused.has_value() && refused.error().code == meios::diagnostic_code::uncontained_asset));
    REQUIRE(resolves_to(meios::load(document, rooted_at(held.root() / "src")), mesh));
#ifdef MEIOS_HAS_YAML
    const std::filesystem::path by_package = held.write("doc/package.xacro", yaml_robot("package://p/cfg.yaml", "pkg"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    REQUIRE_FALSE(meios::load(by_package, rooted_at(share), log).has_value());
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::uncontained_asset) == 1);
    REQUIRE(link_names(meios::load(by_package, rooted_at(held.root() / "src"))) == std::set<std::string>{"pkg_yaml"});
#endif
}

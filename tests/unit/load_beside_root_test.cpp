#include "xacro_read_probe.h"
#include "xacro_include_tree.h"
#include "load_beside_root_fixture.h"

#include <meios/urdf.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <variant>
#include <filesystem>

using beside_root_fixture::rooted;
using beside_root_fixture::linked_sub;
using beside_root_fixture::load_outcome;

namespace
{

std::filesystem::path mesh_document(const include_tree::tree &held, const std::string &filename)
{
    return held.write("doc/robot.xacro", include_tree::robot("<link name=\"base\"><visual><geometry><mesh filename=\"" + filename + "\"/></geometry></visual></link>"));
}

bool resolves_to(const load_outcome &loaded, const std::filesystem::path &expected)
{
    if(!loaded.has_value() || loaded->robot.links.size() != 1 || loaded->robot.links[0].visuals.size() != 1)
        return false;
    const auto *mesh = std::get_if<meios::mesh<>>(&loaded->robot.links[0].visuals[0].geom.shape);
    return mesh != nullptr && mesh->resolved_path && std::filesystem::equivalent(*mesh->resolved_path, expected);
}

}

TEST_CASE("a mesh linked beside its description into a configured root resolves", "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    const std::filesystem::path target   = held.write("root/common/m.stl", "solid m\nendsolid m\n");
    const std::filesystem::path document = mesh_document(held, "m.stl");
    if(!include_tree::linked(target, held.root() / "doc" / "m.stl"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    REQUIRE(resolves_to(meios::load(document, rooted(held, false)), target));
}

TEST_CASE("a mesh spelled back through a linked directory into a configured root resolves where the link leads", "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    const std::filesystem::path target   = held.write("root/common/m.stl", "solid m\nendsolid m\n");
    const std::filesystem::path document = mesh_document(held, "sub/../m.stl");
    if(!linked_sub(held, "root/common"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    REQUIRE(resolves_to(meios::load(document, rooted(held, false)), target));
}

TEST_CASE("a mesh spelled back through a linked directory leading under no configured root refuses unless outside links are followed", "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    std::filesystem::create_directories(held.root() / "root");
    const std::filesystem::path target   = held.write("elsewhere/m.stl", "solid m\nendsolid m\n");
    const std::filesystem::path document = mesh_document(held, "sub/../m.stl");
    if(!linked_sub(held, "elsewhere"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome refused = meios::load(document, rooted(held, false));
    REQUIRE_FALSE(refused.has_value());
    REQUIRE(refused.error().code == meios::diagnostic_code::uncontained_asset);
    REQUIRE(resolves_to(meios::load(document, rooted(held, true)), target));
}

#ifdef MEIOS_HAS_YAML

using beside_root_fixture::loads_link;

namespace
{

std::filesystem::path yaml_document(const include_tree::tree &held, const std::string &spec)
{
    return held.write("doc/robot.xacro", include_tree::robot("<link name=\"${xacro.load_yaml('" + spec + "')['name']}\"/>"));
}

}

TEST_CASE("an auxiliary document linked beside its description into a configured root loads", "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    const std::filesystem::path target   = held.write("root/common/cfg.yaml", "name: from_root_link\n");
    const std::filesystem::path document = yaml_document(held, "cfg.yaml");
    if(!include_tree::linked(target, held.root() / "doc" / "cfg.yaml"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome loaded = meios::load(document, rooted(held, false));
    if(!loaded)
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
    REQUIRE(loads_link(loaded, "from_root_link"));
}

TEST_CASE("an auxiliary document spelled back through a linked directory into a configured root loads where the link leads", "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    held.write("root/common/cfg.yaml", "name: through_link\n");
    held.write("doc/cfg.yaml", "name: lexical\n");
    const std::filesystem::path document = yaml_document(held, "sub/../cfg.yaml");
    if(!linked_sub(held, "root/common"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome loaded = meios::load(document, rooted(held, false));
    if(!loaded)
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
    REQUIRE(loads_link(loaded, "through_link"));
}

TEST_CASE("an auxiliary document spelled back through a linked directory leading under no configured root refuses unless outside links are followed", "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    std::filesystem::create_directories(held.root() / "root");
    held.write("elsewhere/cfg.yaml", "name: through_link\n");
    const std::filesystem::path document = yaml_document(held, "sub/../cfg.yaml");
    if(!linked_sub(held, "elsewhere"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    const load_outcome refused = meios::load(document, rooted(held, false), log);
    REQUIRE_FALSE(refused.has_value());
    REQUIRE(refused.error().code == meios::diagnostic_code::unresolved_asset);
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::uncontained_asset) == 1);
    REQUIRE(loads_link(meios::load(document, rooted(held, true)), "through_link"));
}

#endif

TEST_CASE("a spelling through a directory that does not exist reads the file beside the document", "[urdf][load][link]")
{
    const include_tree::tree held;
    held.write("doc/x.xacro", include_tree::binds("beside"));
    const std::filesystem::path include = held.write("doc/top.xacro", include_tree::reads_leaf("sub/../x.xacro"));
    const std::filesystem::path mesh    = held.write("doc/m.stl", "solid m\nendsolid m\n");

    REQUIRE(beside_root_fixture::loads_link(meios::load(include, meios::load_options{}), "beside"));
    REQUIRE(resolves_to(meios::load(mesh_document(held, "sub/../m.stl"), meios::load_options{}), mesh));
#ifdef MEIOS_HAS_YAML
    held.write("doc/cfg.yaml", "name: beside\n");
    REQUIRE(beside_root_fixture::loads_link(meios::load(yaml_document(held, "sub/../cfg.yaml"), meios::load_options{}), "beside"));
#endif
}

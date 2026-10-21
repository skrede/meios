#include "xacro_include_tree.h"
#include "load_beside_root_fixture.h"

#include <meios/urdf.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <filesystem>

using beside_root_fixture::rooted;
using beside_root_fixture::resolves_to;
using beside_root_fixture::load_outcome;
using beside_root_fixture::mesh_document;

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

#ifdef MEIOS_HAS_YAML

using beside_root_fixture::loads_link;
using beside_root_fixture::yaml_document;

TEST_CASE("an auxiliary document linked beside its description into a configured root loads", "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    const std::filesystem::path target   = held.write("root/common/cfg.yaml", "name: from_root_link\n");
    const std::filesystem::path document = yaml_document(held, "doc/robot.xacro", "cfg.yaml");
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

TEST_CASE("an auxiliary document's leading .. landing under a configured root loads, and refuses with none", "[urdf][load][yaml]")
{
    const include_tree::tree held;
    held.write("root/p/config/c.yaml", "name: from_cfg\n");
    const std::filesystem::path document = yaml_document(held, "root/p/urdf/robot.xacro", "../config/c.yaml");

    REQUIRE(loads_link(meios::load(document, rooted(held, false)), "from_cfg"));
    const load_outcome refused = meios::load(document, meios::load_options{});
    REQUIRE_FALSE(refused.has_value());
    REQUIRE(refused.error().code == meios::diagnostic_code::unresolved_asset);
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
    REQUIRE(beside_root_fixture::loads_link(meios::load(yaml_document(held, "doc/robot.xacro", "sub/../cfg.yaml"), meios::load_options{}), "beside"));
#endif
}

TEST_CASE("a document named by a relative path with a directory resolves its sibling mesh from the parent working directory", "[urdf][load][mesh]")
{
    const include_tree::tree held;
    const std::filesystem::path target = held.write("sub/meshes/base.stl", "solid m\nendsolid m\n");
    held.write("sub/document.urdf", "<robot name=\"probe\"><link name=\"base\"><visual><geometry><mesh filename=\"meshes/base.stl\"/></geometry></visual></link></robot>");
    const std::filesystem::path saved = std::filesystem::current_path();

    std::filesystem::current_path(held.root());
    const load_outcome loaded = meios::load("sub/document.urdf");
    std::filesystem::current_path(saved);

    INFO((loaded ? std::string("loaded") : loaded.error().message));
    REQUIRE(resolves_to(loaded, target));
}

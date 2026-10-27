#include "xacro_include_tree.h"
#include "load_beside_root_fixture.h"
// MEIOS_HAS_YAML comes from meios/config.h, which the fixture header brings in.
#ifdef MEIOS_HAS_YAML
    #include "xacro_read_probe.h"
#endif

#include <meios/urdf.h>

#ifdef MEIOS_HAS_YAML
    #include <meios/diagnostic/log_sink.h>
#endif
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#ifdef MEIOS_HAS_YAML
    #include <vector>
#endif
#include <filesystem>

using beside_root_fixture::rooted;
using beside_root_fixture::linked_sub;
using beside_root_fixture::resolves_to;
using beside_root_fixture::load_outcome;
using beside_root_fixture::mesh_document;

TEST_CASE("a mesh spelled back through a linked directory into a configured root resolves where the link leads on POSIX and is read lexically on Windows", "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    const std::filesystem::path target   = held.write("root/common/m.stl", "solid m\nendsolid m\n");
    const std::filesystem::path beside   = held.write("doc/m.stl", "solid beside\nendsolid beside\n");
    const std::filesystem::path document = mesh_document(held, "sub/../m.stl");
    if(!linked_sub(held, "root/common"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome loaded = meios::load(document, rooted(held, false));
    if constexpr(include_tree::reduces_parent_lexically)
        REQUIRE(resolves_to(loaded, beside));
    else
        REQUIRE(resolves_to(loaded, target));
}

TEST_CASE("a mesh spelled back through a linked directory leading under no configured root refuses unless outside links are followed on POSIX and is read lexically on Windows",
          "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    std::filesystem::create_directories(held.root() / "root");
    const std::filesystem::path target   = held.write("elsewhere/m.stl", "solid m\nendsolid m\n");
    const std::filesystem::path beside   = held.write("doc/m.stl", "solid beside\nendsolid beside\n");
    const std::filesystem::path document = mesh_document(held, "sub/../m.stl");
    if(!linked_sub(held, "elsewhere"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome unset    = meios::load(document, rooted(held, false));
    const load_outcome followed = meios::load(document, rooted(held, true));
    if constexpr(include_tree::reduces_parent_lexically)
    {
        REQUIRE(resolves_to(unset, beside));
        REQUIRE(resolves_to(followed, beside));
    }
    else
    {
        REQUIRE_FALSE(unset.has_value());
        REQUIRE(unset.error().code == meios::diagnostic_code::uncontained_asset);
        REQUIRE(resolves_to(followed, target));
    }
}

TEST_CASE("a mesh spelled back through a linked directory leading under no configured root with nothing beside the document refuses, "
          "as uncontained on POSIX and as unresolved on Windows",
          "[urdf][load][mesh][link]")
{
    const include_tree::tree held;
    std::filesystem::create_directories(held.root() / "root");
    held.write("elsewhere/m.stl", "solid m\nendsolid m\n");
    const std::filesystem::path document = mesh_document(held, "sub/../m.stl");
    if(!linked_sub(held, "elsewhere"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome refused = meios::load(document, rooted(held, false));
    REQUIRE_FALSE(refused.has_value());
    if constexpr(include_tree::reduces_parent_lexically)
        REQUIRE(refused.error().code == meios::diagnostic_code::unresolved_asset);
    else
        REQUIRE(refused.error().code == meios::diagnostic_code::uncontained_asset);
}

#ifdef MEIOS_HAS_YAML

using beside_root_fixture::loads_link;
using beside_root_fixture::yaml_document;

TEST_CASE("an auxiliary document spelled back through a linked directory into a configured root loads where the link leads on POSIX and is read lexically on Windows",
          "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    held.write("root/common/cfg.yaml", "name: through_link\n");
    held.write("doc/cfg.yaml", "name: lexical\n");
    const std::filesystem::path document = yaml_document(held, "doc/robot.xacro", "sub/../cfg.yaml");
    if(!linked_sub(held, "root/common"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome loaded = meios::load(document, rooted(held, false));
    if(!loaded)
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
    if constexpr(include_tree::reduces_parent_lexically)
        REQUIRE(loads_link(loaded, "lexical"));
    else
        REQUIRE(loads_link(loaded, "through_link"));
}

TEST_CASE("an auxiliary document spelled back through a linked directory leading under no configured root refuses unless outside links are followed on POSIX "
          "and is read lexically on Windows",
          "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    std::filesystem::create_directories(held.root() / "root");
    held.write("elsewhere/cfg.yaml", "name: through_link\n");
    held.write("doc/cfg.yaml", "name: lexical\n");
    const std::filesystem::path document = yaml_document(held, "doc/robot.xacro", "sub/../cfg.yaml");
    if(!linked_sub(held, "elsewhere"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    const load_outcome unset    = meios::load(document, rooted(held, false), log);
    const load_outcome followed = meios::load(document, rooted(held, true));
    if constexpr(include_tree::reduces_parent_lexically)
    {
        REQUIRE(loads_link(unset, "lexical"));
        REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::uncontained_asset) == 0);
        REQUIRE(loads_link(followed, "lexical"));
    }
    else
    {
        REQUIRE_FALSE(unset.has_value());
        REQUIRE(unset.error().code == meios::diagnostic_code::unresolved_asset);
        REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::uncontained_asset) == 1);
        REQUIRE(loads_link(followed, "through_link"));
    }
}

TEST_CASE("an auxiliary document spelled back through a linked directory leading under no configured root with nothing beside the document refuses as unresolved, "
          "with one uncontained record on POSIX and none on Windows",
          "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    std::filesystem::create_directories(held.root() / "root");
    held.write("elsewhere/cfg.yaml", "name: through_link\n");
    const std::filesystem::path document = yaml_document(held, "doc/robot.xacro", "sub/../cfg.yaml");
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
    if constexpr(include_tree::reduces_parent_lexically)
        REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::uncontained_asset) == 0);
    else
        REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::uncontained_asset) == 1);
}

#endif

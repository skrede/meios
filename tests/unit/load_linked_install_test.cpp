#include "xacro_read_probe.h"
#include "linked_install_tree.h"
#include "xacro_include_tree.h"

#include <meios/urdf.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>

namespace
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

bool loads_link(const load_outcome &loaded, const std::string &name)
{
    return loaded.has_value() && loaded->robot.links.size() == 1 && loaded->robot.links[0].name == name;
}

meios::load_options following()
{
    meios::load_options opts;
    opts.includes.follow_outside_links = true;
    return opts;
}

linked_install::files include_sources()
{
    return {{"robot.xacro", include_tree::reads_leaf("arm.xacro")}, {"arm.xacro", include_tree::binds("arm")}};
}

}

TEST_CASE("a relative include loads beside a document opened through per-file install links", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_install::per_file_links(held, include_sources(), "robot.xacro");
    if(document.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    REQUIRE(loads_link(meios::load(document), "arm"));
}

TEST_CASE("a relative include loads beside a document opened through a linked install directory", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_install::linked_directory(held, include_sources(), "robot.xacro");
    if(document.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    REQUIRE(loads_link(meios::load(document), "arm"));
}

TEST_CASE("an install link leading into neither directory of its document refuses an include unless outside links are followed", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_install::per_file_links(held, {{"robot.xacro", include_tree::reads_leaf("arm.xacro")}}, "robot.xacro");
    const std::filesystem::path target   = held.write("build/arm.xacro", include_tree::binds("arm"));
    if(document.empty() || linked_install::link_elsewhere(held, "arm.xacro", target).empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome refused = meios::load(document);
    REQUIRE_FALSE(refused.has_value());
    REQUIRE(refused.error().code == meios::diagnostic_code::uncontained_asset);
    REQUIRE(loads_link(meios::load(document, following()), "arm"));
}

#ifdef MEIOS_HAS_YAML

namespace
{

std::string yaml_robot()
{
    return include_tree::robot("<link name=\"${xacro.load_yaml('cfg.yaml')['name']}\"/>");
}

linked_install::files yaml_sources()
{
    return {{"robot_yaml.xacro", yaml_robot()}, {"cfg.yaml", "name: from_yaml\n"}};
}

}

TEST_CASE("a relative auxiliary document loads beside a document opened through a linked install directory", "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_install::linked_directory(held, yaml_sources(), "robot_yaml.xacro");
    if(document.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    REQUIRE(loads_link(meios::load(document), "from_yaml"));
}

TEST_CASE("a relative auxiliary document loads beside a document opened through per-file install links", "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_install::per_file_links(held, yaml_sources(), "robot_yaml.xacro");
    if(document.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    REQUIRE(loads_link(meios::load(document), "from_yaml"));
}

TEST_CASE("an auxiliary document linked into neither directory of its document refuses unless outside links are followed", "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_install::per_file_links(held, {{"robot_yaml.xacro", yaml_robot()}}, "robot_yaml.xacro");
    const std::filesystem::path target   = held.write("build/cfg.yaml", "name: from_build\n");
    if(document.empty() || linked_install::link_elsewhere(held, "cfg.yaml", target).empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    const load_outcome refused = meios::load(document, meios::load_options{}, log);
    REQUIRE_FALSE(refused.has_value());
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::uncontained_asset) == 1);
    REQUIRE(loads_link(meios::load(document, following()), "from_build"));
}

#endif

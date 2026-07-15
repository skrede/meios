#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace
{

std::string slurp(const std::string &name)
{
    const std::filesystem::path path = std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

struct recorder
{
    std::vector<meios::level> &levels;

    void operator()(meios::level lvl, const std::string &) { levels.push_back(lvl); }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &)
    {
        levels.push_back(lvl);
    }
};

struct msg_recorder
{
    std::vector<std::pair<meios::level, std::string>> &entries;

    void operator()(meios::level lvl, const std::string &message)
    {
        entries.emplace_back(lvl, message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        entries.emplace_back(lvl, message);
    }
};

meios::tree<double> parse_fixture(const std::string &name, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources, eval, log, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::strict, {} };
    meios::pod_recorder<meios::tree<double>> rec(log, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp(name), rec);
    return rec.result();
}

}

TEST_CASE("a name-only visual material resolves against the top-level table", "[urdf][material]")
{
    meios::log_sink log;
    const meios::tree<double> robot = parse_fixture("named_material.urdf", log);

    REQUIRE(robot.materials.size() == 1);
    REQUIRE(robot.materials.at(0).name == "blue");
    REQUIRE(robot.materials.at(0).color.has_value());
    REQUIRE(robot.links.at(0).visuals.at(0).material_ref.has_value());
    REQUIRE(robot.links.at(0).visuals.at(0).material_ref.value() == "blue");
}

TEST_CASE("an unresolved material reference stays empty with a warning, never gray", "[urdf][material]")
{
    std::vector<meios::level> levels;
    meios::log_sink_f capture{ recorder{ levels } };
    const meios::tree<double> robot = parse_fixture("unresolved_material.urdf", capture);

    REQUIRE(robot.materials.empty());
    REQUIRE_FALSE(robot.links.at(0).visuals.at(0).material_ref.has_value());
    REQUIRE(std::count(levels.begin(), levels.end(), meios::level::warn) >= 1);
}

TEST_CASE("the real single-link description leaves its name-only ref unresolved", "[urdf][material]")
{
    std::vector<meios::level> levels;
    meios::log_sink_f capture{ recorder{ levels } };
    const meios::tree<double> robot = parse_fixture("test-desc.urdf", capture);

    REQUIRE(robot.name == "test");
    REQUIRE_FALSE(robot.links.at(0).visuals.at(0).material_ref.has_value());
    REQUIRE(std::count(levels.begin(), levels.end(), meios::level::warn) >= 1);
}

TEST_CASE("an inline visual material captures its color and lifts into the table", "[urdf][material]")
{
    meios::log_sink log;
    const meios::tree<double> robot = parse_fixture("inline_color_material.urdf", log);

    const meios::visual<double> &vis = robot.links.at(0).visuals.at(0);
    REQUIRE_FALSE(vis.material_ref.has_value());
    REQUIRE(vis.material_inline.has_value());
    REQUIRE(vis.material_inline->name == "red");
    REQUIRE(vis.material_inline->color.has_value());
    REQUIRE(vis.material_inline->color->r == 1.0);

    REQUIRE(robot.materials.size() == 1);
    REQUIRE(robot.materials.at(0).name == "red");
    REQUIRE(robot.materials.at(0).color.has_value());
}

TEST_CASE("an unnamed inline visual material captures its texture and does not lift", "[urdf][material]")
{
    meios::log_sink log;
    const meios::tree<double> robot = parse_fixture("inline_texture_material.urdf", log);

    const meios::visual<double> &vis = robot.links.at(0).visuals.at(0);
    REQUIRE(vis.material_inline.has_value());
    REQUIRE(vis.material_inline->texture.has_value());
    REQUIRE(vis.material_inline->texture.value() == "skin.png");
    REQUIRE(vis.material_inline->name.empty());
    REQUIRE(robot.materials.empty());
}

TEST_CASE("a dropped robot-level ros2_control child raises a loud WARN naming it", "[urdf][material]")
{
    std::vector<std::pair<meios::level, std::string>> entries;
    meios::log_sink_f capture{ msg_recorder{ entries } };
    parse_fixture("robot_control_child.urdf", capture);

    bool named = false;
    int unhandled = 0;
    for(const std::pair<meios::level, std::string> &entry : entries)
    {
        if(entry.first != meios::level::warn)
            continue;
        if(entry.second.find("unhandled robot-level") != std::string::npos)
            ++unhandled;
        if(entry.second.find("ros2_control") != std::string::npos
           && entry.second.find("system_hw") != std::string::npos)
            named = true;
    }
    REQUIRE(named);
    REQUIRE(unhandled == 3);
}

TEST_CASE("a robot with only material/link children raises no unhandled-element WARN", "[urdf][material]")
{
    std::vector<std::pair<meios::level, std::string>> entries;
    meios::log_sink_f capture{ msg_recorder{ entries } };
    parse_fixture("named_material.urdf", capture);

    for(const std::pair<meios::level, std::string> &entry : entries)
        REQUIRE(entry.second.find("unhandled robot-level") == std::string::npos);
}

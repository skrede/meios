#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace
{

std::filesystem::path fixture_path(const std::string &name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

struct recorder
{
    std::vector<meios::level> &levels;
    std::vector<std::string> &msgs;

    void operator()(meios::level lvl, const std::string &msg)
    {
        levels.push_back(lvl);
        msgs.push_back(msg);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &msg)
    {
        levels.push_back(lvl);
        msgs.push_back(msg);
    }
};

}

TEST_CASE("meios::load parses the real single-link description", "[urdf][realfixture]")
{
    std::vector<meios::level> levels;
    std::vector<std::string> msgs;
    meios::log_sink_f capture{ recorder{ levels, msgs } };
    meios::load_options opts;

    const meios::model<double> robot = meios::load(fixture_path("test-desc.urdf"), opts, capture);

    REQUIRE(robot.name == "test");
    REQUIRE(robot.links.size() == 1);
    REQUIRE(robot.links.at(0).name == "base_link");
}

TEST_CASE("the name-only 'blue' material warns and never resolves to a color", "[urdf][realfixture]")
{
    std::vector<meios::level> levels;
    std::vector<std::string> msgs;
    meios::log_sink_f capture{ recorder{ levels, msgs } };
    meios::load_options opts;

    const meios::model<double> robot = meios::load(fixture_path("test-desc.urdf"), opts, capture);

    REQUIRE(robot.materials.empty());
    REQUIRE_FALSE(robot.links.at(0).visuals.at(0).material_ref.has_value());
    REQUIRE(std::count(levels.begin(), levels.end(), meios::level::warn) == 1);

    bool warned_blue = false;
    for(const std::string &msg : msgs)
        if(msg.find("blue") != std::string::npos)
            warned_blue = true;
    REQUIRE(warned_blue);
}

TEST_CASE("a non-<robot> root is a loud diagnostic, never a silent misparse", "[urdf][realfixture]")
{
    std::vector<meios::level> levels;
    std::vector<std::string> msgs;
    meios::log_sink_f capture{ recorder{ levels, msgs } };
    meios::load_options opts;

    const meios::model<double> robot = meios::load(fixture_path("not_a_robot.xml"), opts, capture);

    REQUIRE(robot.links.empty());
    REQUIRE(std::count(levels.begin(), levels.end(), meios::level::error) >= 1);

    bool complained_about_root = false;
    for(const std::string &msg : msgs)
        if(msg.find("robot") != std::string::npos)
            complained_about_root = true;
    REQUIRE(complained_about_root);
}

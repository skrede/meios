#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
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

meios::tree<double> parse_fixture(const std::string &name, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources, eval, log, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::strict };
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

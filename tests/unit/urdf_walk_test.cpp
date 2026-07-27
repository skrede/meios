#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

static_assert(meios::format_reader<meios::urdf_reader>);
static_assert(meios::model_sink<meios::world_recorder>);

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

int root_count(const std::vector<int> &parent_of)
{
    int roots = 0;
    for(int parent : parent_of)
        if(parent == -1)
            ++roots;
    return roots;
}

}

TEST_CASE("the streaming walk pushes a branched robot into a pod_recorder", "[urdf][walk]")
{
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::log_sink log;
    meios::parse_context ctx{ sources, eval, log, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::fail, {} };
    meios::pod_recorder<meios::tree<double>> rec(log, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp("branched_all_joints.urdf"), rec);
    const meios::tree<double> &robot = rec.result();

    REQUIRE(robot.name == "branched_all_joints");
    REQUIRE(robot.links.size() == 7);
    REQUIRE(robot.joints.size() == 6);
    REQUIRE(root_count(robot.parent_of) == 1);
    REQUIRE(robot.parent_of.at(0) == -1);

    std::set<meios::joint_kind> kinds;
    for(const meios::joint<double> &edge : robot.joints)
        kinds.insert(edge.kind);
    REQUIRE(kinds.size() == 6);

    REQUIRE(robot.links.at(0).origin_loc.has_value());
    REQUIRE(robot.joints.at(0).origin_loc.has_value());
    REQUIRE(robot.links.at(0).origin_loc->line > 0);
}

TEST_CASE("a non-finite numeric attribute is rejected with a loud diagnostic", "[urdf][walk]")
{
    std::vector<meios::level> levels;
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::log_sink_f capture{ recorder{ levels } };
    meios::parse_context ctx{ sources, eval, capture, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::fail, {} };
    meios::pod_recorder<meios::tree<double>> rec(capture, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp("nonfinite_origin.urdf"), rec);

    REQUIRE_FALSE(levels.empty());
    REQUIRE(std::count(levels.begin(), levels.end(), meios::level::error) >= 1);
}

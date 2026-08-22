#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>
#include <meios/bundle.h>

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

std::filesystem::path fixture(const std::string &name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

std::string slurp(const std::string &name)
{
    std::ifstream in(fixture(name), std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

struct robot_capture
{
    meios::robot_info info;

    void on_robot(const meios::robot_info &robot) { info = robot; }
    void on_material(const meios::material<double> &) {}
    void on_link(const meios::link<double> &) {}
    void on_joint(const meios::joint<double> &) {}
    void finish() {}
};

meios::robot_info read_robot(const std::string &name)
{
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::log_sink log;
    meios::parse_context ctx{ sources, eval, log, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::fail, {} };
    robot_capture capture;
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp(name), capture);
    return capture.info;
}

// The saved document opens with an XML declaration that also spells version=, so the
// round trip is only observable on the robot element's own start tag.
std::string written_robot_element(const meios::robot_info &robot)
{
    std::ostringstream out;
    meios::log_sink log;
    meios::urdf_writer writer(out, log);
    writer.on_robot(robot);
    writer.finish();
    const std::string document = out.str();
    const std::string::size_type at = document.find("<robot");
    return document.substr(at, document.find('>', at) - at);
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

TEST_CASE("a document that declares no version records none", "[urdf][walk]")
{
    REQUIRE_FALSE(read_robot("branched_all_joints.urdf").version.has_value());
}

TEST_CASE("a document declaring the supported version records it", "[urdf][walk]")
{
    const meios::robot_info robot = read_robot("profile/version_supported.urdf");

    REQUIRE(robot.version.has_value());
    REQUIRE(robot.version.value() == "1.0");
}

TEST_CASE("a document declaring any other version is refused at a real location", "[urdf][walk]")
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(fixture("profile/version_unsupported.urdf"));

    REQUIRE_FALSE(loaded);
    REQUIRE(loaded.error().code == meios::diagnostic_code::unsupported_version);
    REQUIRE_FALSE(loaded.error().loc.file.empty());
    REQUIRE(loaded.error().loc.line > 0);
}

TEST_CASE("the version attribute is recognized rather than reported as unknown", "[urdf][walk]")
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(fixture("profile/version_supported.urdf"));

    REQUIRE(loaded);
    REQUIRE(meios::has(loaded->claims, meios::completeness::parsed));
}

TEST_CASE("a recorded version survives a write back out, and an absent one is not invented",
          "[urdf][walk]")
{
    const std::string declared =
        written_robot_element(read_robot("profile/version_supported.urdf"));
    const std::string silent = written_robot_element(read_robot("profile/origin_xyz_ok.urdf"));

    REQUIRE(declared.find("version=\"1.0\"") != std::string::npos);
    REQUIRE(silent.find("version=") == std::string::npos);
}

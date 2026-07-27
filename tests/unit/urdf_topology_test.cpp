#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <meios/model/topology.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstddef>
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

struct entry
{
    meios::level lvl;
    meios::diagnostic_code code;
    int line;
    std::string msg;
};

struct capture
{
    std::vector<entry> &entries;

    void operator()(meios::level lvl, const std::string &msg)
    {
        entries.push_back({ lvl, meios::diagnostic_code::unspecified, 0, msg });
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &msg)
    {
        entries.push_back({ lvl, meios::diagnostic_code::unspecified, loc.line, msg });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &msg)
    {
        entries.push_back({ lvl, code, loc.line, msg });
    }
};

meios::tree<double> parse(const std::string &fixture)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources, eval, silent, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::fail, {} };
    meios::pod_recorder<meios::tree<double>> rec(silent, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp(fixture), rec);
    return rec.result();
}

bool located(const std::vector<entry> &entries, meios::level lvl, meios::diagnostic_code expected)
{
    for(const entry &e : entries)
        if(e.lvl == lvl && e.line > 0 && e.code == expected)
            return true;
    return false;
}

meios::topology_result reconstruct(const meios::tree<double> &robot, std::vector<entry> &entries,
                                   meios::topology_policy policy)
{
    meios::log_sink_f cap{ capture{ entries } };
    return meios::reconstruct_topology(robot.links, robot.joints, cap, policy);
}

int root_count(const std::vector<int> &parent_of)
{
    int roots = 0;
    for(int parent : parent_of)
        if(parent == -1)
            ++roots;
    return roots;
}

meios::tree<double> two_root_forest()
{
    meios::tree<double> robot;
    robot.name = "two_roots";
    robot.links = { { .name = "r0" }, { .name = "r0_a" }, { .name = "r0_b" },
                    { .name = "r1" }, { .name = "r1_a" } };
    robot.joints = { { .name = "j0", .parent = "r0", .child = "r0_a" },
                     { .name = "j1", .parent = "r0", .child = "r0_b" },
                     { .name = "j2", .parent = "r1", .child = "r1_a" } };
    return robot;
}

int count_matching(const std::vector<entry> &entries, meios::diagnostic_code code)
{
    int hits = 0;
    for(const entry &e : entries)
        if(e.code == code)
            ++hits;
    return hits;
}

}

TEST_CASE("each broken class is a located error under topology_policy::fail", "[urdf][topology]")
{
    struct expectation
    {
        std::string fixture;
        meios::diagnostic_code code;
    };
    for(const expectation &exp :
        { expectation{ "multi_root.urdf", meios::diagnostic_code::additional_root },
          expectation{ "cycle.urdf", meios::diagnostic_code::link_on_cycle },
          expectation{ "orphan_joint.urdf", meios::diagnostic_code::undeclared_link },
          expectation{ "unreachable_link.urdf", meios::diagnostic_code::unreachable_link },
          expectation{ "multi_parent.urdf", meios::diagnostic_code::multiple_parents } })
    {
        std::vector<entry> entries;
        const meios::topology_result result =
            reconstruct(parse(exp.fixture), entries, meios::topology_policy::fail);
        REQUIRE_FALSE(result.ok);
        REQUIRE(located(entries, meios::level::error, exp.code));
    }
}

TEST_CASE("a valid branched tree reconstructs with a single root", "[urdf][topology]")
{
    std::vector<entry> entries;
    const meios::tree<double> robot = parse("branched_all_joints.urdf");
    const meios::topology_result result = reconstruct(robot, entries, meios::topology_policy::fail);

    REQUIRE(result.ok);
    REQUIRE(entries.empty());
    REQUIRE(root_count(result.topo.parent_of) == 1);
    REQUIRE(result.topo.parent_of.at(0) == -1);
}

TEST_CASE("warn downgrades to a warning while skip stays silent", "[urdf][topology]")
{
    const meios::tree<double> robot = parse("multi_root.urdf");

    std::vector<entry> warned;
    const meios::topology_result warn = reconstruct(robot, warned, meios::topology_policy::warn);
    REQUIRE(warn.ok);
    REQUIRE(located(warned, meios::level::warn, meios::diagnostic_code::additional_root));

    std::vector<entry> skipped;
    const meios::topology_result skip = reconstruct(robot, skipped, meios::topology_policy::skip);
    REQUIRE(skip.ok);
    REQUIRE(skipped.empty());
}

TEST_CASE("a valid multi-root forest reports no false unreachable under warn", "[urdf][topology]")
{
    const meios::tree<double> robot = two_root_forest();

    std::vector<entry> entries;
    const meios::topology_result result = reconstruct(robot, entries, meios::topology_policy::warn);

    REQUIRE(result.ok);
    REQUIRE(count_matching(entries, meios::diagnostic_code::unreachable_link) == 0);
}

TEST_CASE("joint_of maps each child to its forming joint and roots to -1", "[urdf][topology]")
{
    const meios::tree<double> robot = two_root_forest();

    std::vector<entry> entries;
    const meios::topology_result result = reconstruct(robot, entries, meios::topology_policy::skip);

    REQUIRE(result.topo.joint_of == std::vector<int>{ -1, 0, 1, -1, 2 });
}

TEST_CASE("order is the all-roots DFS pre-order, roots and children ascending", "[urdf][topology]")
{
    const meios::tree<double> robot = two_root_forest();

    std::vector<entry> entries;
    const meios::topology_result result = reconstruct(robot, entries, meios::topology_policy::skip);

    REQUIRE(result.topo.order == std::vector<int>{ 0, 1, 2, 3, 4 });
}

TEST_CASE("order is frozen public data and does not silently drift", "[urdf][topology]")
{
    const meios::tree<double> robot = two_root_forest();

    std::vector<entry> first_log;
    std::vector<entry> second_log;
    const std::vector<int> first =
        reconstruct(robot, first_log, meios::topology_policy::skip).topo.order;
    const std::vector<int> second =
        reconstruct(robot, second_log, meios::topology_policy::skip).topo.order;

    REQUIRE(first == second);
    REQUIRE(first == std::vector<int>{ 0, 1, 2, 3, 4 });
}

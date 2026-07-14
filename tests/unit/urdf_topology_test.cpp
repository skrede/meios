#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <meios/model/topology.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstddef>
#include <filesystem>
#include <string_view>

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
    int line;
    std::string msg;
};

struct capture
{
    std::vector<entry> &entries;

    void operator()(meios::level lvl, const std::string &msg) { entries.push_back({ lvl, 0, msg }); }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &msg)
    {
        entries.push_back({ lvl, loc.line, msg });
    }
};

meios::tree<double> parse(const std::string &fixture)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources, eval, silent, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::strict, {} };
    meios::pod_recorder<meios::tree<double>> rec(silent, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp(fixture), rec);
    return rec.result();
}

bool located(const std::vector<entry> &entries, meios::level lvl, std::string_view needle)
{
    for(const entry &e : entries)
        if(e.lvl == lvl && e.line > 0 && e.msg.find(needle) != std::string::npos)
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

}

TEST_CASE("each broken class is a located error under topology_policy::fail", "[urdf][topology]")
{
    struct expectation
    {
        std::string fixture;
        std::string needle;
    };
    for(const expectation &exp :
        { expectation{ "multi_root.urdf", "additional root" },
          expectation{ "cycle.urdf", "cycle" },
          expectation{ "orphan_joint.urdf", "undeclared link" },
          expectation{ "unreachable_link.urdf", "unreachable" },
          expectation{ "multi_parent.urdf", "more than one parent" } })
    {
        std::vector<entry> entries;
        const meios::topology_result result =
            reconstruct(parse(exp.fixture), entries, meios::topology_policy::fail);
        REQUIRE_FALSE(result.ok);
        REQUIRE(located(entries, meios::level::error, exp.needle));
    }
}

TEST_CASE("a valid branched tree reconstructs with a single root", "[urdf][topology]")
{
    std::vector<entry> entries;
    const meios::tree<double> robot = parse("branched_all_joints.urdf");
    const meios::topology_result result = reconstruct(robot, entries, meios::topology_policy::fail);

    REQUIRE(result.ok);
    REQUIRE(entries.empty());
    REQUIRE(root_count(result.parent_of) == 1);
    REQUIRE(result.parent_of.at(0) == -1);
}

TEST_CASE("warn downgrades to a warning while skip stays silent", "[urdf][topology]")
{
    const meios::tree<double> robot = parse("multi_root.urdf");

    std::vector<entry> warned;
    const meios::topology_result warn = reconstruct(robot, warned, meios::topology_policy::warn);
    REQUIRE(warn.ok);
    REQUIRE(located(warned, meios::level::warn, "additional root"));

    std::vector<entry> skipped;
    const meios::topology_result skip = reconstruct(robot, skipped, meios::topology_policy::skip);
    REQUIRE(skip.ok);
    REQUIRE(skipped.empty());
}

#include "urdf_topology_fixture.h"

#include <meios/io.h>
#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <meios/model/topology.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace
{

std::string slurp(const std::string &name)
{
    const std::filesystem::path path = std::filesystem::path{MEIOS_URDF_FIXTURE_DIR} / name;
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

meios::tree<double> parse(const std::string &fixture)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{sources, eval, silent, meios::missing_asset::warn, meios::topology_policy::fail, meios::material_policy::warn, meios::strictness::fail, {}};
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

int root_count(const std::vector<int> &parent_of)
{
    int roots = 0;
    for(int parent : parent_of)
        if(parent == -1)
            ++roots;
    return roots;
}

}

TEST_CASE("a valid branched tree reconstructs with a single root", "[urdf][topology]")
{
    std::vector<entry> entries;
    const meios::tree<double> robot     = parse("branched_all_joints.urdf");
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

// The dangling link reference this loop used to carry left the graph layer: it is a
// property of the document text, refused before a record exists and at every setting.
TEST_CASE("the graph policy governs each genuine graph property at each of its settings", "[urdf][topology]")
{
    struct expectation
    {
        std::string fixture;
        meios::diagnostic_code code;
    };
    for(const expectation &exp :
        {expectation{"multi_root.urdf", meios::diagnostic_code::additional_root}, expectation{"cycle.urdf", meios::diagnostic_code::link_on_cycle},
         expectation{"unreachable_link.urdf", meios::diagnostic_code::unreachable_link}, expectation{"multi_parent.urdf", meios::diagnostic_code::multiple_parents}})
    {
        INFO(exp.fixture);
        const meios::tree<double> robot = parse(exp.fixture);

        std::vector<entry> failed;
        REQUIRE_FALSE(reconstruct(robot, failed, meios::topology_policy::fail).ok);
        REQUIRE(located(failed, meios::level::error, exp.code));

        std::vector<entry> warned;
        REQUIRE(reconstruct(robot, warned, meios::topology_policy::warn).ok);
        REQUIRE(located(warned, meios::level::warn, exp.code));

        std::vector<entry> skipped;
        REQUIRE(reconstruct(robot, skipped, meios::topology_policy::skip).ok);
        REQUIRE(skipped.empty());
    }
}

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <meios/model/topology.h>

#include <rapidcheck.h>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>

namespace
{

std::string kind_name(int kind)
{
    switch(kind)
    {
    case 1: return "revolute";
    case 2: return "continuous";
    case 3: return "prismatic";
    case 4: return "floating";
    case 5: return "planar";
    default: return "fixed";
    }
}

bool needs_axis(int kind)
{
    return kind == 1 || kind == 2 || kind == 3 || kind == 5;
}

bool needs_limit(int kind)
{
    return kind == 1 || kind == 3;
}

std::string emit_urdf(int links, const std::vector<int> &parent, const std::vector<int> &kind)
{
    std::string out = "<robot name=\"gen\">\n";
    for(int i = 0; i < links; ++i)
        out += "  <link name=\"link" + std::to_string(i) + "\"/>\n";
    for(int i = 1; i < links; ++i)
    {
        const int k = kind[static_cast<std::size_t>(i)];
        out += "  <joint name=\"joint" + std::to_string(i) + "\" type=\"" + kind_name(k) + "\">\n";
        out += "    <parent link=\"link" + std::to_string(parent[static_cast<std::size_t>(i)]) + "\"/>\n";
        out += "    <child link=\"link" + std::to_string(i) + "\"/>\n";
        out += "    <origin xyz=\"0 0 0.1\" rpy=\"0 0 0\"/>\n";
        if(needs_axis(k))
            out += "    <axis xyz=\"0 0 1\"/>\n";
        if(needs_limit(k))
            out += "    <limit effort=\"10\" velocity=\"1\"/>\n";
        out += "  </joint>\n";
    }
    return out + "</robot>\n";
}

meios::tree<double> parse(const std::string &text, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources, eval, log, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::fail, "gen.urdf" };
    meios::pod_recorder<meios::tree<double>> rec(log, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(text, rec);
    return rec.result();
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

TEST_CASE("a generated well-formed branched tree parses and round-trips", "[urdf][property]")
{
    REQUIRE(rc::check("names round-trip and topology reconstructs to a single-root tree", [] {
        const int links = *rc::gen::inRange<int>(2, 9);
        std::vector<int> parent(static_cast<std::size_t>(links), -1);
        std::vector<int> kind(static_cast<std::size_t>(links), 0);
        for(int i = 1; i < links; ++i)
        {
            parent[static_cast<std::size_t>(i)] = *rc::gen::inRange<int>(0, i);
            kind[static_cast<std::size_t>(i)] = *rc::gen::inRange<int>(0, 6);
        }

        meios::log_sink silent;
        const meios::tree<double> robot = parse(emit_urdf(links, parent, kind), silent);

        RC_ASSERT(robot.links.size() == static_cast<std::size_t>(links));
        RC_ASSERT(robot.joints.size() == static_cast<std::size_t>(links - 1));
        for(int i = 0; i < links; ++i)
            RC_ASSERT(robot.links[static_cast<std::size_t>(i)].name == "link" + std::to_string(i));
        for(int i = 1; i < links; ++i)
            RC_ASSERT(robot.joints[static_cast<std::size_t>(i - 1)].name == "joint" + std::to_string(i));

        const meios::topology_result topo =
            meios::reconstruct_topology(robot.links, robot.joints, silent, meios::topology_policy::fail);
        RC_ASSERT(topo.ok);
        RC_ASSERT(root_count(topo.topo.parent_of) == 1);
    }));
}

#include <meios/bundle.h>
#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <meios/model/topology.h>

#include <rapidcheck.h>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <sstream>
#include <cstddef>

namespace
{

struct capture_log : meios::log_sink
{
    using meios::log_sink::log;

    std::vector<meios::level> levels;

    void log(meios::level lvl, const std::string &) override
    {
        levels.push_back(lvl);
    }

    void log(meios::level lvl, const meios::source_location &, const std::string &) override
    {
        levels.push_back(lvl);
    }

    bool saw(meios::level want) const
    {
        for(meios::level lvl : levels)
            if(lvl == want)
                return true;
        return false;
    }
};

meios::joint_kind kind_of(int k)
{
    switch(k)
    {
    case 1: return meios::joint_kind::revolute;
    case 2: return meios::joint_kind::continuous;
    case 3: return meios::joint_kind::prismatic;
    case 4: return meios::joint_kind::floating;
    case 5: return meios::joint_kind::planar;
    default: return meios::joint_kind::fixed;
    }
}

// Adversarial-but-XML-1.0-legal tokens that force the emitter's escaping path:
// an ampersand, an angle bracket, a double quote, and a non-ASCII codepoint (U+03C0).
std::string flavor(int i)
{
    switch(i % 4)
    {
    case 0: return "&";
    case 1: return "<";
    case 2: return "\"";
    default: return "\xCF\x80";
    }
}

std::string link_name(int i)
{
    return i == 0 ? std::string("base") : flavor(i) + "link" + std::to_string(i);
}

std::string joint_name(int i)
{
    return flavor(i) + "joint" + std::to_string(i);
}

std::vector<meios::link<double>> build_links(int n)
{
    std::vector<meios::link<double>> links;
    for(int i = 0; i < n; ++i)
    {
        meios::link<double> node{};
        node.name = link_name(i);
        links.push_back(node);
    }
    return links;
}

std::vector<meios::joint<double>> build_joints(const std::vector<int> &parent, const std::vector<int> &kind)
{
    std::vector<meios::joint<double>> joints;
    for(int i = 1; i < static_cast<int>(parent.size()); ++i)
    {
        meios::joint<double> edge{};
        edge.name = joint_name(i);
        edge.kind = kind_of(kind[static_cast<std::size_t>(i)]);
        edge.parent = link_name(parent[static_cast<std::size_t>(i)]);
        edge.child = link_name(i);
        edge.axis = meios::vector3<double>{ 0.0, 0.0, 1.0 };
        edge.limits = meios::joint_limits<double>{ -1.0, 1.0, 10.0, 1.0 };
        joints.push_back(edge);
    }
    return joints;
}

meios::tree<double> build_model(const std::vector<int> &parent, const std::vector<int> &kind)
{
    meios::tree<double> robot;
    robot.name = "gen";
    robot.links = build_links(static_cast<int>(parent.size()));
    robot.joints = build_joints(parent, kind);
    return robot;
}

std::string emit(const meios::tree<double> &robot, meios::log_sink &log)
{
    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    meios::replay(robot, writer);
    return out.str();
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

TEST_CASE("a generated model round-trips through the shared emitter", "[bundle][property]")
{
    REQUIRE(rc::check("names, kinds and topology survive parse->flatten->parse", [] {
        const int links = *rc::gen::inRange<int>(11, 20);
        std::vector<int> parent(static_cast<std::size_t>(links), -1);
        std::vector<int> kind(static_cast<std::size_t>(links), 0);
        for(int i = 1; i < links; ++i)
        {
            parent[static_cast<std::size_t>(i)] = *rc::gen::inRange<int>(0, i);
            kind[static_cast<std::size_t>(i)] = i <= 6 ? i - 1 : *rc::gen::inRange<int>(0, 6);
        }

        meios::log_sink silent;
        const meios::tree<double> model = build_model(parent, kind);
        const meios::tree<double> round = parse(emit(model, silent), silent);

        RC_ASSERT(round.links.size() == static_cast<std::size_t>(links));
        RC_ASSERT(round.joints.size() == static_cast<std::size_t>(links - 1));
        for(int i = 0; i < links; ++i)
            RC_ASSERT(round.links[static_cast<std::size_t>(i)].name == link_name(i));
        for(int i = 1; i < links; ++i)
        {
            RC_ASSERT(round.joints[static_cast<std::size_t>(i - 1)].name == joint_name(i));
            RC_ASSERT(round.joints[static_cast<std::size_t>(i - 1)].kind
                      == kind_of(kind[static_cast<std::size_t>(i)]));
        }

        const meios::topology_result topo =
            meios::reconstruct_topology(round.links, round.joints, silent, meios::topology_policy::fail);
        RC_ASSERT(topo.ok);
        RC_ASSERT(root_count(topo.topo.parent_of) == 1);
    }));
}

TEST_CASE("the emitter itself hard-fails on XML-1.0-unencodable content", "[bundle][property]")
{
    capture_log log;
    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    meios::tree<double> robot;
    robot.name = "gen";
    meios::link<double> arm{};
    arm.name = std::string("arm") + '\x01';
    robot.links.push_back(arm);
    meios::replay(robot, writer);

    REQUIRE(writer.status().status == meios::emit_status::xml_unencodable);
    REQUIRE(log.saw(meios::level::error));
    REQUIRE(out.str().empty());
}

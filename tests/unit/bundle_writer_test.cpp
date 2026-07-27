#include <meios/bundle.h>
#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <sstream>

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

meios::tree<double> sample_tree()
{
    meios::tree<double> robot;
    robot.name = "gen";

    meios::material<double> mat{};
    mat.name = "red";
    mat.color = meios::rgba<double>{ 1.0, 0.0, 0.0, 1.0 };
    mat.texture = std::string("tex.png");
    robot.materials.push_back(mat);

    meios::link<double> base{};
    base.name = "base";
    meios::visual<double> vis{};
    vis.geom.shape = meios::box<double>{ meios::vector3<double>{ 1.0, 2.0, 3.0 } };
    base.visuals.push_back(vis);
    robot.links.push_back(base);

    meios::link<double> tool{};
    tool.name = "tool";
    robot.links.push_back(tool);

    meios::joint<double> edge{};
    edge.name = "j1";
    edge.kind = meios::joint_kind::revolute;
    edge.parent = "base";
    edge.child = "tool";
    edge.axis = meios::vector3<double>{ 0.0, 0.0, 1.0 };
    robot.joints.push_back(edge);

    return robot;
}

meios::tree<double> reparse(const std::string &text, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources, eval, log, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::fail, "emit.urdf" };
    meios::pod_recorder<meios::tree<double>> rec(log, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(text, rec);
    return rec.result();
}

}

TEST_CASE("emit_status stringifies for the diagnostic channel", "[bundle][writer]")
{
    REQUIRE(meios::to_string(meios::emit_status::ok) == "ok");
    REQUIRE(meios::to_string(meios::emit_status::xml_unencodable) == "xml_unencodable");
    REQUIRE(meios::to_string(meios::emit_status::io_error) == "io_error");
}

TEST_CASE("a resolved tree replays to a single well-formed flat urdf", "[bundle][writer]")
{
    capture_log log;
    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    const meios::tree<double> robot = sample_tree();
    meios::replay(robot, writer);

    REQUIRE(writer.status().status == meios::emit_status::ok);
    const std::string urdf = out.str();
    REQUIRE(urdf.find("<robot") != std::string::npos);
    REQUIRE(urdf.find("<material") != std::string::npos);
    REQUIRE(urdf.find("<link") != std::string::npos);
    REQUIRE(urdf.find("<box") != std::string::npos);
    REQUIRE(urdf.find("<joint") != std::string::npos);
    REQUIRE(urdf.find("<axis") != std::string::npos);

    capture_log relog;
    const meios::tree<double> round = reparse(urdf, relog);
    REQUIRE(round.links.size() == robot.links.size());
    REQUIRE(round.joints.size() == robot.joints.size());
    REQUIRE(round.joints[0].kind == meios::joint_kind::revolute);
}

TEST_CASE("special characters in a name round-trip through escaping", "[bundle][writer]")
{
    capture_log log;
    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    meios::tree<double> robot;
    robot.name = "gen";
    meios::link<double> arm{};
    arm.name = "arm&<\"x";
    robot.links.push_back(arm);
    meios::replay(robot, writer);
    REQUIRE(writer.status().status == meios::emit_status::ok);

    capture_log relog;
    const meios::tree<double> round = reparse(out.str(), relog);
    REQUIRE(round.links.size() == 1);
    REQUIRE(round.links[0].name == "arm&<\"x");
}

TEST_CASE("an un-encodable control character hard-fails the emitter", "[bundle][writer]")
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

TEST_CASE("an overlong UTF-8 encoding hard-fails the emitter", "[bundle][writer]")
{
    capture_log log;
    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    meios::tree<double> robot;
    robot.name = "gen";
    meios::link<double> arm{};
    arm.name = std::string("arm") + '\xC0' + '\xBC';
    robot.links.push_back(arm);
    meios::replay(robot, writer);

    REQUIRE(writer.status().status == meios::emit_status::xml_unencodable);
    REQUIRE(log.saw(meios::level::error));
    REQUIRE(out.str().empty());
}

TEST_CASE("an inline visual material survives read->flatten with its color verbatim", "[bundle][writer]")
{
    capture_log log;
    const char *src =
        "<robot name=\"r\">"
        "<link name=\"base\"><visual><geometry><box size=\"1 1 1\"/></geometry>"
        "<material name=\"red\"><color rgba=\"1 0 0 1\"/></material></visual></link>"
        "</robot>";
    const meios::tree<double> robot = reparse(src, log);

    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    meios::replay(robot, writer);

    REQUIRE(writer.status().status == meios::emit_status::ok);
    REQUIRE(out.str().find("color rgba") != std::string::npos);
}

TEST_CASE("an empty name is preserved with a warning, not a failure", "[bundle][writer]")
{
    capture_log log;
    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    meios::tree<double> robot;
    robot.name = "gen";
    meios::link<double> arm{};
    arm.name = "";
    robot.links.push_back(arm);
    meios::replay(robot, writer);

    REQUIRE(writer.status().status == meios::emit_status::ok);
    REQUIRE(log.saw(meios::level::warn));
    REQUIRE_FALSE(out.str().empty());
}

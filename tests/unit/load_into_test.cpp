#include <meios/io.h>
#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <filesystem>

namespace
{

std::filesystem::path fixture(const std::string &name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

// A sink whose every observation is inspectable, so the failure path can assert that
// none of them moved rather than only that the completion callback stayed away.
struct counting_sink
{
    int robots = 0;
    int materials = 0;
    int links = 0;
    int joints = 0;
    bool finished = false;
    std::string name;

    void on_robot(const meios::robot_info &robot)
    {
        ++robots;
        name = robot.name;
    }
    void on_material(const meios::material<double> &) { ++materials; }
    void on_link(const meios::link<double> &) { ++links; }
    void on_joint(const meios::joint<double> &) { ++joints; }
    void finish() { finished = true; }
};

}

TEST_CASE("a valid description drives every sink callback once per record", "[urdf][load_into]")
{
    counting_sink sink;
    const meios::load_summary summary =
        meios::load_into(fixture("branched_all_joints.urdf"), meios::load_options{}, sink);

    REQUIRE(sink.robots == 1);
    REQUIRE(sink.name == "branched_all_joints");
    REQUIRE(sink.links == 7);
    REQUIRE(sink.joints == 6);
    REQUIRE(sink.finished);
    REQUIRE(has(summary.claims, meios::completeness::topology_valid));
}

TEST_CASE("declared materials reach the sink", "[urdf][load_into]")
{
    counting_sink sink;
    meios::load_into(fixture("named_material.urdf"), meios::load_options{}, sink);

    REQUIRE(sink.materials == 1);
    REQUIRE(sink.links == 1);
}

TEST_CASE("a failed load leaves every counter on the sink at zero", "[urdf][load_into]")
{
    counting_sink sink;
    const meios::load_summary summary =
        meios::load_into(fixture("cycle.urdf"), meios::load_options{}, sink);

    REQUIRE(sink.robots == 0);
    REQUIRE(sink.materials == 0);
    REQUIRE(sink.links == 0);
    REQUIRE(sink.joints == 0);
    REQUIRE(sink.name.empty());
    REQUIRE_FALSE(sink.finished);
    REQUIRE(summary.claims == meios::completeness::none);
}

TEST_CASE("an unopenable path leaves the sink untouched too", "[urdf][load_into]")
{
    counting_sink sink;
    meios::log_sink log;
    meios::load_into(fixture("does_not_exist.urdf"), meios::load_options{}, sink, log);

    REQUIRE(sink.robots == 0);
    REQUIRE(sink.materials == 0);
    REQUIRE(sink.links == 0);
    REQUIRE(sink.joints == 0);
    REQUIRE_FALSE(sink.finished);
}

TEST_CASE("the failure summary carries the diagnostics the load accumulated", "[urdf][load_into]")
{
    counting_sink sink;
    const meios::load_summary summary =
        meios::load_into(fixture("cycle.urdf"), meios::load_options{}, sink);

    const meios::expected<meios::load_result, meios::load_error> direct =
        meios::load(fixture("cycle.urdf"), meios::load_options{});

    REQUIRE_FALSE(direct.has_value());
    REQUIRE_FALSE(summary.diagnostics.empty());
    REQUIRE(summary.diagnostics.size() == direct.error().diagnostics.size());
}

TEST_CASE("the success summary agrees with the entry point it delegates to", "[urdf][load_into]")
{
    counting_sink sink;
    const meios::load_summary summary =
        meios::load_into(fixture("test-desc.urdf"), meios::load_options{}, sink);

    const meios::expected<meios::load_result, meios::load_error> direct =
        meios::load(fixture("test-desc.urdf"), meios::load_options{});

    REQUIRE(direct.has_value());
    REQUIRE_FALSE(summary.diagnostics.empty());
    REQUIRE(summary.diagnostics.size() == direct->diagnostics.size());
    REQUIRE(summary.claims == direct->claims);
}

TEST_CASE("the summary spells its members as the full result does", "[urdf][load_into]")
{
    const meios::expected<meios::load_result, meios::load_error> direct =
        meios::load(fixture("test-desc.urdf"), meios::load_options{});
    REQUIRE(direct.has_value());

    meios::load_summary summary;
    summary.diagnostics = direct->diagnostics;
    summary.claims = direct->claims;

    REQUIRE(summary.diagnostics.size() == direct->diagnostics.size());
    REQUIRE(summary.claims == direct->claims);
}

TEST_CASE("one sink type serves the facade and the parse entry point", "[urdf][load_into]")
{
    counting_sink from_file;
    meios::load_into(fixture("named_material.urdf"), meios::load_options{}, from_file);

    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::log_sink log;
    meios::parse_context ctx{ sources,          eval,
                              log,              meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::fail,      {} };
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    counting_sink from_buffer;
    parser.parse(R"(<?xml version="1.0"?><robot name="named_material"><link name="base_link"/></robot>)",
                 from_buffer);

    REQUIRE(from_file.name == from_buffer.name);
    REQUIRE(from_file.links == from_buffer.links);
}

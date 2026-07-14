#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <vector>

namespace
{

struct stub_reader
{
    using model = meios::tree<double, meios::rpy<double>>;
};

template <typename Sink>
void feed(Sink &sink)
{
    sink.on_robot({ .name = "arm" });
    sink.on_material({ .name = "grey" });
    sink.on_link({ .name = "base" });
    sink.on_link({ .name = "link1" });
    sink.on_joint({ .name = "j1",
                    .kind = meios::joint_kind::revolute,
                    .parent = "base",
                    .child = "link1" });
    sink.finish();
}

}

static_assert(meios::model_sink<meios::pod_recorder<meios::tree<double>>>);
static_assert(meios::model_sink<meios::world_recorder>);
static_assert(meios::format_reader<stub_reader>);

TEST_CASE("a pod_recorder assembles a rooted tree from the record stream", "[model][sink]")
{
    meios::log_sink log;
    meios::pod_recorder<meios::tree<double>> recorder(log, meios::topology_policy::fail);
    feed(recorder);
    const meios::tree<double> &arm = recorder.result();

    REQUIRE(arm.name == "arm");
    REQUIRE(arm.links.size() == 2);
    REQUIRE(arm.joints.size() == 1);
    REQUIRE(arm.materials.size() == 1);
    REQUIRE(arm.materials.at(0).name == "grey");
    REQUIRE(arm.parent_of.size() == 2);
    REQUIRE(arm.parent_of.at(0) == -1);
    REQUIRE(arm.parent_of.at(1) == 0);
}

TEST_CASE("a world_recorder builds a dynamic tree model with name maps", "[model][sink]")
{
    meios::log_sink log;
    meios::world_recorder recorder(log, meios::topology_policy::fail);
    feed(recorder);
    const meios::model<double> &arm = recorder.result();

    REQUIRE(arm.kind == meios::structure::tree);
    REQUIRE(arm.links.size() == 2);
    REQUIRE(arm.materials.size() == 1);
    REQUIRE(arm.materials.at(0).name == "grey");
    REQUIRE(arm.link_index.at("base") == 0);
    REQUIRE(arm.link_index.at("link1") == 1);
    REQUIRE(arm.joint_index.at("j1") == 0);
}

TEST_CASE("reconstruct_topology reports a broken tree through the log under fail", "[model][topology]")
{
    std::vector<meios::link<double>> links{ { .name = "a" }, { .name = "b" } };
    std::vector<meios::joint<double>> joints;

    std::ostringstream out;
    meios::log_sink_s log{ out };
    const meios::topology_result result =
        meios::reconstruct_topology(links, joints, log, meios::topology_policy::fail);

    REQUIRE_FALSE(result.ok);
    REQUIRE(result.parent_of.size() == 2);
    REQUIRE_FALSE(out.str().empty());
}

TEST_CASE("reconstruct_topology accepts a valid branched tree", "[model][topology]")
{
    std::vector<meios::link<double>> links{ { .name = "root" }, { .name = "l" }, { .name = "r" } };
    std::vector<meios::joint<double>> joints{
        { .name = "jl", .parent = "root", .child = "l" },
        { .name = "jr", .parent = "root", .child = "r" }
    };

    std::ostringstream out;
    meios::log_sink_s log{ out };
    const meios::topology_result result =
        meios::reconstruct_topology(links, joints, log, meios::topology_policy::fail);

    REQUIRE(result.ok);
    REQUIRE(result.parent_of.at(0) == -1);
    REQUIRE(result.parent_of.at(1) == 0);
    REQUIRE(result.parent_of.at(2) == 0);
    REQUIRE(out.str().empty());
}

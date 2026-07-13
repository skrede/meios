#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

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
    meios::pod_recorder<meios::tree<double>> recorder;
    feed(recorder);
    const meios::tree<double> &arm = recorder.result();

    REQUIRE(arm.name == "arm");
    REQUIRE(arm.links.size() == 2);
    REQUIRE(arm.joints.size() == 1);
    REQUIRE(arm.parent_of.size() == 2);
    REQUIRE(arm.parent_of.at(0) == -1);
    REQUIRE(arm.parent_of.at(1) == 0);
}

TEST_CASE("a world_recorder builds a dynamic tree model with name maps", "[model][sink]")
{
    meios::world_recorder recorder;
    feed(recorder);
    const meios::model<double> &arm = recorder.result();

    REQUIRE(arm.kind == meios::structure::tree);
    REQUIRE(arm.links.size() == 2);
    REQUIRE(arm.link_index.at("base") == 0);
    REQUIRE(arm.link_index.at("link1") == 1);
    REQUIRE(arm.joint_index.at("j1") == 0);
}

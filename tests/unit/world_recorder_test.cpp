#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

#include <sstream>

TEST_CASE("a located topology failure is retained as ok=false and a first-failure location",
          "[model][sink]")
{
    std::ostringstream out;
    meios::log_sink_s log{ out };
    meios::world_recorder recorder(log, meios::topology_policy::fail);

    recorder.on_robot({ .name = "arm" });
    recorder.on_link({ .name = "a" });
    recorder.on_link({ .name = "b", .origin_loc = meios::source_location{ "robot.urdf", 7, 3 } });
    recorder.finish();

    REQUIRE_FALSE(recorder.ok());
    REQUIRE(recorder.first_failure().has_value());
    REQUIRE(recorder.first_failure()->line == 7);
    REQUIRE(recorder.first_failure()->column == 3);
    REQUIRE_FALSE(out.str().empty());
}

TEST_CASE("a valid model is ok, has no first failure, and moves out with a populated topology",
          "[model][sink]")
{
    std::ostringstream out;
    meios::log_sink_s log{ out };
    meios::world_recorder recorder(log, meios::topology_policy::fail);

    recorder.on_robot({ .name = "arm" });
    recorder.on_link({ .name = "base" });
    recorder.on_link({ .name = "link1" });
    recorder.on_joint({ .name = "j1",
                        .kind = meios::joint_kind::revolute,
                        .parent = "base",
                        .child = "link1" });
    recorder.finish();

    REQUIRE(recorder.ok());
    REQUIRE_FALSE(recorder.first_failure().has_value());
    REQUIRE(out.str().empty());

    const meios::model<double> &view = recorder.result();
    REQUIRE(view.topo.parent_of.size() == 2);
    REQUIRE(view.topo.parent_of.at(1) == 0);

    const meios::model<double> moved = recorder.take_model();
    REQUIRE(moved.links.size() == 2);
    REQUIRE(moved.topo.order.size() == 2);
    REQUIRE(moved.topo.order.front() == 0);
}

#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <sstream>
#include <utility>
#include <optional>

namespace
{

// Terminal sink that records the (code, location) of the first error it receives on
// the code-carrying overload, so a test can assert the code survives the relay chain.
class capture_sink final : public meios::log_sink
{
public:
    using meios::log_sink::log;

    void log(meios::level lvl, meios::diagnostic_code code, const meios::source_location &location,
             const std::string &message) override
    {
        (void)message;
        if(lvl == meios::level::error && !m_first)
            m_first = std::pair{ code, location };
    }

    const std::optional<std::pair<meios::diagnostic_code, meios::source_location>> &first() const
    {
        return m_first;
    }

private:
    std::optional<std::pair<meios::diagnostic_code, meios::source_location>> m_first;
};

}

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

TEST_CASE("a typed topology diagnostic keeps its code and location through the world_recorder relay",
          "[model][sink][diagnostic]")
{
    capture_sink sink;
    meios::world_recorder recorder(sink, meios::topology_policy::fail);

    recorder.on_robot({ .name = "arm" });
    recorder.on_link({ .name = "a", .origin_loc = meios::source_location{ "robot.urdf", 3, 1 } });
    recorder.on_link({ .name = "b", .origin_loc = meios::source_location{ "robot.urdf", 9, 5 } });
    recorder.finish();

    REQUIRE_FALSE(recorder.ok());
    REQUIRE(sink.first().has_value());
    REQUIRE(sink.first()->first == meios::diagnostic_code::additional_root);
    REQUIRE(sink.first()->second.file == "robot.urdf");
    REQUIRE(sink.first()->second.line == 9);
    REQUIRE(sink.first()->second.column == 5);
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

TEST_CASE("the name index and the linear scan answer with the same index", "[model][sink]")
{
    std::ostringstream out;
    meios::log_sink_s log{ out };
    meios::world_recorder recorder(log, meios::topology_policy::skip);

    recorder.on_robot({ .name = "arm" });
    for(const char *name : { "base", "link1", "link2" })
        recorder.on_link({ .name = name });
    recorder.on_joint({ .name = "j1", .parent = "base", .child = "link1" });
    recorder.on_joint({ .name = "j2", .parent = "link1", .child = "link2" });
    recorder.finish();

    const meios::model<double> &view = recorder.result();
    REQUIRE(recorder.ok());
    REQUIRE(view.link_index.size() == view.links.size());
    REQUIRE(view.joint_index.size() == view.joints.size());
    for(const std::pair<const std::string, int> &named : view.link_index)
        REQUIRE(named.second == meios::detail::index_of(view.links, named.first));
    for(const std::pair<const std::string, int> &named : view.joint_index)
        REQUIRE(named.second == meios::detail::index_of(view.joints, named.first));
}

TEST_CASE("a model whose index and records disagree on size is reported, not resolved",
          "[model][sink]")
{
    capture_sink sink;
    meios::world_recorder recorder(sink, meios::topology_policy::skip);

    recorder.on_robot({ .name = "arm" });
    recorder.on_link({ .name = "twin" });
    recorder.on_link({ .name = "twin" });
    recorder.finish();

    REQUIRE(recorder.result().links.size() == 2);
    REQUIRE(recorder.result().link_index.size() == 1);
    REQUIRE_FALSE(recorder.ok());
    REQUIRE(sink.first().has_value());
    REQUIRE(sink.first()->first == meios::diagnostic_code::duplicate_name);
}

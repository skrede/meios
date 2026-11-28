#include "urdf_topology_fixture.h"

#include <meios/model.h>

#include <meios/model/topology.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{

meios::tree<double> two_root_forest()
{
    meios::tree<double> robot;
    robot.name   = "two_roots";
    robot.links  = {{.name = "r0"}, {.name = "r0_a"}, {.name = "r0_b"}, {.name = "r1"}, {.name = "r1_a"}};
    robot.joints = {{.name = "j0", .parent = "r0", .child = "r0_a"}, {.name = "j1", .parent = "r0", .child = "r0_b"}, {.name = "j2", .parent = "r1", .child = "r1_a"}};
    return robot;
}

int count_matching(const std::vector<entry> &entries, meios::diagnostic_code code)
{
    int hits = 0;
    for(const entry &e : entries)
        if(e.code == code)
            ++hits;
    return hits;
}

}

TEST_CASE("a valid multi-root forest reports no false unreachable under warn", "[urdf][topology]")
{
    const meios::tree<double> robot = two_root_forest();

    std::vector<entry> entries;
    const meios::topology_result result = reconstruct(robot, entries, meios::topology_policy::warn);

    REQUIRE(result.ok);
    REQUIRE(count_matching(entries, meios::diagnostic_code::unreachable_link) == 0);
}

TEST_CASE("joint_of maps each child to its forming joint and roots to -1", "[urdf][topology]")
{
    const meios::tree<double> robot = two_root_forest();

    std::vector<entry> entries;
    const meios::topology_result result = reconstruct(robot, entries, meios::topology_policy::skip);

    REQUIRE(result.topo.joint_of == std::vector<int>{-1, 0, 1, -1, 2});
}

TEST_CASE("order is the all-roots DFS pre-order, roots and children ascending", "[urdf][topology]")
{
    const meios::tree<double> robot = two_root_forest();

    std::vector<entry> entries;
    const meios::topology_result result = reconstruct(robot, entries, meios::topology_policy::skip);

    REQUIRE(result.topo.order == std::vector<int>{0, 1, 2, 3, 4});
}

TEST_CASE("order is frozen public data and does not silently drift", "[urdf][topology]")
{
    const meios::tree<double> robot = two_root_forest();

    std::vector<entry> first_log;
    std::vector<entry> second_log;
    const std::vector<int> first  = reconstruct(robot, first_log, meios::topology_policy::skip).topo.order;
    const std::vector<int> second = reconstruct(robot, second_log, meios::topology_policy::skip).topo.order;

    REQUIRE(first == second);
    REQUIRE(first == std::vector<int>{0, 1, 2, 3, 4});
}

#include <meios/model/structure.h>
#include <meios/model/tree.h>
#include <meios/model/closed_chain.h>
#include <meios/model/model.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <variant>
#include <type_traits>

static_assert(std::is_aggregate_v<meios::tree<double>>);
static_assert(std::is_aggregate_v<meios::tree<float>>);
static_assert(std::is_aggregate_v<meios::closed_chain<double>>);
static_assert(std::is_aggregate_v<meios::closed_chain<float>>);
static_assert(std::is_aggregate_v<meios::model<double>>);
static_assert(std::is_aggregate_v<meios::model<float>>);

static_assert(std::is_same_v<meios::tree<double>, meios::tree<double, meios::rpy<double>>>);
static_assert(!std::is_same_v<meios::tree<double>, meios::tree<double, meios::quaternion<double>>>);
static_assert(std::is_same_v<decltype(meios::tree<double>{}.parent_of), std::vector<int>>);

TEST_CASE("structure enumerates the two kinematic topologies", "[model][types]")
{
    REQUIRE(meios::structure::tree != meios::structure::closed_chain);
    REQUIRE(static_cast<int>(meios::structure::tree) == 0);
    REQUIRE(static_cast<int>(meios::structure::closed_chain) == 1);
}

TEST_CASE("a tree wires a single root and one child by index", "[model][types]")
{
    const meios::tree<double> arm{
        .name = "arm",
        .links = { { .name = "base" }, { .name = "link1" } },
        .joints = { { .name = "j1",
                      .kind = meios::joint_kind::revolute,
                      .parent = "base",
                      .child = "link1" } },
        .parent_of = { -1, 0 },
    };

    REQUIRE(arm.links.size() == 2);
    REQUIRE(arm.joints.size() == 1);
    REQUIRE(arm.parent_of.at(0) == -1);
    REQUIRE(arm.parent_of.at(1) == 0);
    REQUIRE(arm.links.at(static_cast<std::size_t>(arm.parent_of.at(1))).name == "base");
}

TEST_CASE("a closed_chain carries a tree base plus first-class loop constraints", "[model][types]")
{
    const meios::closed_chain<double> mech{
        .base = { .name = "four_bar", .parent_of = { -1, 0, 1 } },
        .loops = { { .parent = "link3", .child = "base" } },
    };

    REQUIRE(mech.base.name == "four_bar");
    REQUIRE(mech.base.parent_of.size() == 3);
    REQUIRE(mech.loops.size() == 1);
    REQUIRE(mech.loops.front().parent == "link3");
    REQUIRE(mech.loops.front().child == "base");
}

TEST_CASE("a dynamic model addresses links by name through the index map", "[model][types]")
{
    meios::model<double> m{ .name = "arm", .kind = meios::structure::tree };
    m.links.push_back({ .name = "base" });
    m.links.push_back({ .name = "link1" });
    m.link_index["base"] = 0;
    m.link_index["link1"] = 1;

    REQUIRE(m.kind == meios::structure::tree);
    REQUIRE(m.loops.empty());
    REQUIRE(m.link_index.at("link1") == 1);
    REQUIRE(m.links.at(static_cast<std::size_t>(m.link_index.at("base"))).name == "base");
}

TEST_CASE("a dynamic model's rotation variant defaults to rpy and is reassignable", "[model][types]")
{
    meios::model<double> m{};
    REQUIRE(m.rotation.index() == 0);
    REQUIRE(std::holds_alternative<meios::rpy<double>>(m.rotation));

    m.rotation = meios::quaternion<double>{ .w = 1.0, .x = 0.0, .y = 0.0, .z = 0.0 };
    REQUIRE(std::holds_alternative<meios::quaternion<double>>(m.rotation));
}

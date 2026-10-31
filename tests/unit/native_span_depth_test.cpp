#include "ceiling_probe.h"

#include "meios/xacro/deferred_read.h"

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>

using ceiling::load;
using ceiling::loaded;
using ceiling::resolve;
using ceiling::admitted;
using ceiling::refused_on_depth;

// A command ends at its first closing parenthesis, as the reference's pattern ends it, so however
// deep the input the outermost level refuses on the command left open inside it.
TEST_CASE("an argument command written inside another refuses at its first level", "[native][depth]")
{
    const ceiling::resolved deep = resolve(ceiling::nested_arguments(20'000));

    CHECK_FALSE(deep.survived);
    REQUIRE(deep.messages.size() == 1);
    CHECK(deep.messages.front().find("a command's text holds '$('") != std::string::npos);
}

TEST_CASE("nested expression spans refuse one level past what the ceiling admits", "[native][depth]")
{
    const std::size_t most = admitted(meios::detail::nested_scan_cost);

    CHECK(resolve(ceiling::nested_expressions(most)).survived);
    CHECK_FALSE(resolve(ceiling::nested_expressions(most + 1)).survived);
}

// Each level is released by the unwind that abandons it, so spans written side by side cost what
// one of them costs rather than accumulating across the whole attribute.
TEST_CASE("sibling spans do not accumulate against the nesting ceiling", "[native][depth]")
{
    std::string side_by_side;
    for(std::size_t at = 0; at < 200; ++at)
        side_by_side += ceiling::nested_expressions(1) + " ";

    CHECK(resolve(side_by_side).survived);
}

TEST_CASE("a chain of deferred reads refuses one level past what the ceiling admits", "[native][depth]")
{
    const std::size_t most = admitted(meios::detail::deferred_read_cost);

    CHECK(load(ceiling::deferred_chain(most)).rendered);
    CHECK(refused_on_depth(load(ceiling::deferred_chain(most + 1))));
}

TEST_CASE("a circular definition refuses as circular rather than on the depth ceiling", "[native][depth]")
{
    const loaded cycle = load("<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
                              "<xacro:property name=\"a\" value=\"${b}\"/>\n"
                              "<xacro:property name=\"b\" value=\"${a}\"/>\n"
                              "<link name=\"${a}\"/>\n</robot>");

    CHECK_FALSE(cycle.rendered);
    CHECK(cycle.refused == meios::diagnostic_code::xacro_structural_error);
    CHECK(cycle.errors == 1);
}

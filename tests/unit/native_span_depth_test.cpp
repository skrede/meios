#include "ceiling_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>

namespace
{

// The count each shape is admitted to, read off the one ceiling and the cost a level of that
// shape is charged against it.
std::size_t admitted(std::size_t cost)
{
    return meios::default_expression_depth_ceiling / cost;
}

ceiling::resolved resolve(const std::string &raw)
{
    const meios::evaluator_limits defaults;
    meios::detail::eval_session session(defaults);
    meios::eval_scope scope;
    scope.set("n", meios::value{std::string("n")});
    return ceiling::substitute(raw, meios::eval_policy::fail, session, scope);
}

}

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

// Measured by driving each shape until the process died on a 512 KiB thread: 2 448 recursive
// grammar entries and 880 nested scans. One ceiling covers both because a level is charged what it
// costs in grammar entries rather than one apiece, and this is the headroom that buys -- the same
// ninefold margin the grammar axis was sized to.
TEST_CASE("the one nesting ceiling leaves every shape the same headroom", "[native][depth]")
{
    constexpr std::size_t grammar_entries_exhausting = 2448;
    constexpr std::size_t nested_scans_exhausting = 880;

    CHECK(meios::default_expression_depth_ceiling * 9 < grammar_entries_exhausting);
    CHECK(admitted(meios::detail::nested_scan_cost) * 9 < nested_scans_exhausting);
}

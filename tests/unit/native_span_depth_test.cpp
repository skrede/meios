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
    return ceiling::substitute(raw, meios::eval_policy::fail, session);
}

}

// An argument's default is resolved through a substitution context of its own. While the nesting
// depth was held on that context it restarted at every argument boundary, so this shape ran to
// stack exhaustion instead of refusing; the depth is carried on the session for exactly this.
TEST_CASE("an argument nested inside its own default refuses instead of exhausting the stack",
          "[native][depth]")
{
    const ceiling::resolved deep = resolve(ceiling::nested_arguments(20'000));

    CHECK_FALSE(deep.survived);
    REQUIRE(deep.messages.size() == 1);
    CHECK(deep.messages.front().find("expression depth") != std::string::npos);
}

TEST_CASE("nested argument defaults refuse one level past what the ceiling admits",
          "[native][depth]")
{
    const std::size_t most = admitted(meios::detail::nested_substitution_cost);

    CHECK(resolve(ceiling::nested_arguments(most)).survived);
    CHECK_FALSE(resolve(ceiling::nested_arguments(most + 1)).survived);
}

TEST_CASE("nested command spans refuse one level past what the ceiling admits", "[native][depth]")
{
    const std::size_t most = admitted(meios::detail::nested_scan_cost);

    CHECK(resolve(ceiling::nested_commands(most)).survived);
    CHECK_FALSE(resolve(ceiling::nested_commands(most + 1)).survived);
}

// Each level is released by the unwind that abandons it, so spans written side by side cost what
// one of them costs rather than accumulating across the whole attribute.
TEST_CASE("sibling spans do not accumulate against the nesting ceiling", "[native][depth]")
{
    std::string side_by_side;
    for(std::size_t at = 0; at < 200; ++at)
        side_by_side += ceiling::nested_arguments(1) + " ";

    CHECK(resolve(side_by_side).survived);
}

// Measured by driving each shape until the process died on a 512 KiB thread: 2 448 recursive
// grammar entries, 880 nested scans and 229 nested substitutions. One ceiling covers all three
// because a level is charged what it costs in grammar entries rather than one apiece, and this
// is the headroom that buys -- the same ninefold margin the grammar axis was sized to.
TEST_CASE("the one nesting ceiling leaves every shape the same headroom", "[native][depth]")
{
    constexpr std::size_t grammar_entries_exhausting = 2448;
    constexpr std::size_t nested_scans_exhausting = 880;
    constexpr std::size_t nested_substitutions_exhausting = 229;

    CHECK(meios::default_expression_depth_ceiling * 9 < grammar_entries_exhausting);
    CHECK(admitted(meios::detail::nested_scan_cost) * 9 < nested_scans_exhausting);
    CHECK(admitted(meios::detail::nested_substitution_cost) * 9 < nested_substitutions_exhausting);
}

#include "ceiling_probe.h"

#include "meios/xacro/deferred_read.h"

#include <meios/io/source_stack.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <functional>
#include <string_view>

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

struct loaded
{
    bool rendered;
    std::optional<meios::diagnostic_code> refused;
    std::size_t errors;
};

loaded load(const std::string &document)
{
    ceiling::recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    meios::eval_scope scope;
    meios::source_stack sources{};
    const meios::expected<meios::expansion, meios::expansion_error> out =
        meios::expand(document, scope, sources, "inline.xacro", meios::expansion_limits{}, sink);
    if(out)
        return loaded{ true, std::nullopt, heard.messages.size() };
    return loaded{ false, out.error().code, heard.messages.size() };
}

// Lengthened until it refuses, and stopped well short of the length that exhausts the stack.
std::size_t longest_chain(std::string_view open, std::string_view close)
{
    std::size_t levels = 0;
    while(levels < 64 && load(ceiling::deferred_chain(levels + 1, open, close)).rendered)
        ++levels;
    return levels;
}

bool refused_on_depth(const loaded &out)
{
    return !out.rendered && out.refused == meios::diagnostic_code::expansion_budget_exceeded && out.errors == 1;
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

TEST_CASE("a chain of deferred reads refuses one level past what the ceiling admits", "[native][depth]")
{
    const std::size_t most = admitted(meios::detail::deferred_read_cost);

    CHECK(load(ceiling::deferred_chain(most)).rendered);
    CHECK(refused_on_depth(load(ceiling::deferred_chain(most + 1))));
}

// The reading parser's frames stay live beneath the resolution it starts, so its nesting is charged
// to the level that resolution enters.
TEST_CASE("a deferred chain read from inside parentheses refuses at fewer levels", "[native][depth]")
{
    const std::size_t most = longest_chain("", " + 1");

    CHECK(longest_chain("(((", " + 1)))") < most);
    CHECK(refused_on_depth(load(ceiling::deferred_chain(most, "(((", " + 1)))"))));
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

// Measured by driving each chain until the process died on a 512 KiB thread: 175 reads of `${p}`,
// 80 of `${p + 1}`, and two read from inside a hundred parentheses. The longest chain the ceiling
// admits is counted rather than derived from the charge, so a change to either side shows here.
TEST_CASE("a deferred chain keeps the headroom every other nested shape has", "[native][depth]")
{
    constexpr std::size_t bare_reads_exhausting = 175;
    constexpr std::size_t arithmetic_reads_exhausting = 80;
    constexpr std::size_t parenthesized_reads_exhausting = 2;
    const std::string open(100, '(');

    CHECK(longest_chain("", "") * 9 < bare_reads_exhausting);
    CHECK(longest_chain("", " + 1") * 9 < arithmetic_reads_exhausting);
    CHECK(longest_chain(open, " + 1" + std::string(100, ')')) * 9 < parenthesized_reads_exhausting);
}

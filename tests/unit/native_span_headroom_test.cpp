#include "stack_meter.h"
#include "ceiling_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <functional>
#include <string_view>

using ceiling::load;
using ceiling::resolve;
using ceiling::admitted;
using ceiling::deferred_chain;
using ceiling::refused_on_depth;

namespace
{

constexpr std::size_t thread_stack = 512 * 1024;

// Lengthened until it refuses or reaches the cap, which each caller sets one past the length it
// pins, so a chain that grew reads one level too long instead of running on toward the stack's end.
std::size_t longest_chain(std::string_view open, std::string_view close, std::size_t cap)
{
    std::size_t levels = 0;
    while(levels < cap && load(deferred_chain(levels + 1, open, close)).rendered)
        ++levels;
    return levels;
}

// Called after a case's counts and before its metered assertion: a skip raised inside an assertion
// is reported as an unexpected exception.
void skip_unless_metered()
{
    if(!stack_meter::headroom_measured)
        SKIP("the stack figures belong to an unoptimized, uninstrumented POSIX build");
}

// The empty run goes once unmetered first, since a first run initializes statics whose frames
// would otherwise count against the fixed cost alone.
bool keeps_ninefold(const std::function<bool()> &empty, const std::function<bool()> &longest)
{
    bool rendered                           = empty();
    const std::optional<std::size_t> base   = stack_meter::stack_used(thread_stack, [&] { rendered = empty() && rendered; });
    const std::optional<std::size_t> loaded = stack_meter::stack_used(thread_stack, [&] { rendered = longest() && rendered; });
    REQUIRE(base);
    REQUIRE(loaded);
    CHECK(rendered);
    UNSCOPED_INFO("stack used with no level " << *base << " bytes, with the admitted length " << *loaded);
    return stack_meter::keeps_ninefold(*base, *loaded, thread_stack);
}

bool keeps_ninefold(std::string_view open, std::string_view close, std::size_t levels)
{
    return keeps_ninefold([&] { return load(deferred_chain(0, open, close)).rendered; }, [&] { return load(deferred_chain(levels, open, close)).rendered; });
}

std::string parenthesized(std::size_t depth)
{
    return "${" + std::string(depth, '(') + "n" + std::string(depth, ')') + "}";
}

std::size_t deepest_parentheses(std::size_t cap)
{
    std::size_t depth = 0;
    while(depth < cap && resolve(parenthesized(depth + 1)).survived)
        ++depth;
    return depth;
}

std::string opening(std::size_t depth)
{
    return std::string(depth, '(');
}

std::string closing(std::size_t depth)
{
    return " + 1" + std::string(depth, ')');
}

}

TEST_CASE("nested parentheses keep ninefold headroom at the deepest the ceiling admits", "[native][depth][headroom]")
{
    CHECK(deepest_parentheses(15) == 14);
    CHECK_FALSE(resolve(parenthesized(15)).survived);
    skip_unless_metered();
    CHECK(keeps_ninefold([] { return resolve(parenthesized(0)).survived; }, [] { return resolve(parenthesized(14)).survived; }));
}

TEST_CASE("nested expression spans keep ninefold headroom at the deepest the ceiling admits", "[native][depth][headroom]")
{
    const std::size_t most = admitted(meios::detail::nested_scan_cost);

    CHECK(most == 47);
    skip_unless_metered();
    CHECK(keeps_ninefold([] { return resolve(ceiling::nested_expressions(0)).survived; }, [most] { return resolve(ceiling::nested_expressions(most)).survived; }));
}

TEST_CASE("a chain of bare deferred reads keeps ninefold headroom", "[native][depth][headroom]")
{
    CHECK(longest_chain("", "", 10) == 9);
    CHECK(refused_on_depth(load(deferred_chain(10))));
    skip_unless_metered();
    CHECK(keeps_ninefold("", "", 9));
}

TEST_CASE("a chain of deferred reads inside arithmetic keeps ninefold headroom", "[native][depth][headroom]")
{
    CHECK(longest_chain("", closing(0), 6) == 5);
    CHECK(refused_on_depth(load(deferred_chain(6, "", closing(0)))));
    skip_unless_metered();
    CHECK(keeps_ninefold("", closing(0), 5));
}

TEST_CASE("deferred reads inside one parenthesis keep ninefold headroom", "[native][depth][headroom]")
{
    CHECK(longest_chain(opening(1), closing(1), 5) == 4);
    CHECK(refused_on_depth(load(deferred_chain(5, opening(1), closing(1)))));
    skip_unless_metered();
    CHECK(keeps_ninefold(opening(1), closing(1), 4));
}

TEST_CASE("deferred reads inside three parentheses keep ninefold headroom", "[native][depth][headroom]")
{
    CHECK(longest_chain(opening(3), closing(3), 3) == 2);
    CHECK(refused_on_depth(load(deferred_chain(3, opening(3), closing(3)))));
    skip_unless_metered();
    CHECK(keeps_ninefold(opening(3), closing(3), 2));
}

TEST_CASE("deferred reads inside five parentheses keep ninefold headroom", "[native][depth][headroom]")
{
    CHECK(longest_chain(opening(5), closing(5), 3) == 2);
    CHECK(refused_on_depth(load(deferred_chain(3, opening(5), closing(5)))));
    skip_unless_metered();
    CHECK(keeps_ninefold(opening(5), closing(5), 2));
}

TEST_CASE("deferred reads inside ten parentheses keep ninefold headroom", "[native][depth][headroom]")
{
    CHECK(longest_chain(opening(10), closing(10), 2) == 1);
    CHECK(refused_on_depth(load(deferred_chain(2, opening(10), closing(10)))));
    skip_unless_metered();
    CHECK(keeps_ninefold(opening(10), closing(10), 1));
}

TEST_CASE("deferred reads inside thirteen parentheses keep ninefold headroom", "[native][depth][headroom]")
{
    CHECK(longest_chain(opening(13), closing(13), 2) == 1);
    CHECK(refused_on_depth(load(deferred_chain(2, opening(13), closing(13)))));
    skip_unless_metered();
    CHECK(keeps_ninefold(opening(13), closing(13), 1));
}

// The reading parser's nesting and the levels beneath it share the one ceiling, so a read from
// inside this many parentheses has no room left for the level it would start.
TEST_CASE("a deferred read inside fourteen or more parentheses refuses at its first level", "[native][depth][headroom]")
{
    for(const std::size_t depth : {std::size_t{14}, std::size_t{15}, std::size_t{19}, std::size_t{100}})
    {
        INFO(depth << " parentheses");
        CHECK(longest_chain(opening(depth), closing(depth), 1) == 0);
        CHECK(refused_on_depth(load(deferred_chain(1, opening(depth), closing(depth)))));
    }
}

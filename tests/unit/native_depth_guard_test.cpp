#include "ceiling_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <string_view>

namespace
{

constexpr std::size_t past_the_ceiling = 300;

// Every axis but nesting is raised far past what these shapes need, so the depth ceiling is the
// only one that can answer.
meios::evaluator_limits depth_binding()
{
    return meios::evaluator_limits{0, 0, 0, 100'000'000, 100'000'000, 0, 0, 0, 1'000'000};
}

std::string repeated(std::string_view piece, std::size_t count)
{
    std::string text;
    for(std::size_t at = 0; at < count; ++at)
        text += piece;
    return text;
}

ceiling::outcome run(const std::string &expression, std::size_t &deepest)
{
    const meios::evaluator_limits limits = depth_binding();
    meios::detail::eval_session session(limits);
    const ceiling::outcome ran = ceiling::evaluate(expression, session);
    deepest                    = session.counters.expression_depth;
    return ran;
}

void refuses_on_depth(const std::string &shallow, const std::string &deep)
{
    std::size_t deepest = 0;
    INFO("shallow: " << shallow.substr(0, 60));
    CHECK_FALSE(run(shallow, deepest).failed);
    INFO("deep: " << deep.substr(0, 60));
    CHECK(run(deep, deepest).refused_on("expression depth ceiling"));
}

}

TEST_CASE("a run of negations refuses on the nesting ceiling", "[native][depth]")
{
    refuses_on_depth(repeated("not ", 4) + "True", repeated("not ", past_the_ceiling) + "True");
}

TEST_CASE("nested call arguments refuse on the nesting ceiling", "[native][depth]")
{
    refuses_on_depth(repeated("max(1, ", 4) + "1" + repeated(")", 4), repeated("max(1, ", past_the_ceiling) + "1" + repeated(")", past_the_ceiling));
}

TEST_CASE("nested subscripts refuse on the nesting ceiling", "[native][depth]")
{
    refuses_on_depth(repeated("dict(a='a')[", 4) + "'a'" + repeated("]", 4), repeated("dict(a='a')[", past_the_ceiling) + "'a'" + repeated("]", past_the_ceiling));
}

TEST_CASE("a chained conditional refuses on the nesting ceiling", "[native][depth]")
{
    refuses_on_depth(repeated("1 if False else ", 4) + "2", repeated("1 if False else ", past_the_ceiling) + "2");
}

// The same count of groups written side by side rather than inside one another must survive, and
// the high-water mark must stay at what one group costs: a level that is entered and never
// released would accumulate across the siblings and refuse them.
TEST_CASE("sibling groups release their level and stay far under the nesting ceiling", "[native][depth]")
{
    std::size_t siblings_deepest    = 0;
    std::size_t single_deepest      = 0;
    const ceiling::outcome siblings = run(repeated("(1)+", past_the_ceiling) + "(1)", siblings_deepest);
    REQUIRE_FALSE(run("(1)+(1)", single_deepest).failed);

    REQUIRE(single_deepest > 0);
    CHECK_FALSE(siblings.failed);
    CHECK(siblings.rendered == std::to_string(past_the_ceiling + 1));
    CHECK(siblings_deepest <= 2 * single_deepest);
    CHECK(siblings_deepest < meios::default_expression_depth_ceiling / 4);
    CHECK(run(repeated("(", past_the_ceiling) + "1" + repeated(")", past_the_ceiling), siblings_deepest).refused_on("expression depth ceiling"));
}

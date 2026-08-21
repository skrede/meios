#include "ceiling_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>

namespace
{

meios::evaluator_limits collection(std::size_t elements)
{
    return meios::evaluator_limits{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, elements };
}

ceiling::outcome once(std::string_view expression, const meios::evaluator_limits &ceilings)
{
    meios::detail::eval_session session(ceilings);
    return ceiling::evaluate(expression, session);
}

std::string separator_only(std::size_t separators)
{
    return "'" + std::string(separators, ',') + "'.split(',')";
}

}

// A separator-only literal is lexed into one token and its value is read off that token's own
// leaf, so it costs nothing on the byte axis and nothing on the string-length axis; each field it
// yields is empty and costs nothing on either as well. The element axis is the only one that
// moves, which is why an input denoting a million fields is answered by this ceiling alone.
TEST_CASE("a separator-heavy split refuses at the element bound, not after the input",
          "[native][elements]")
{
    constexpr std::size_t bound = 100;
    constexpr std::size_t separators = 1'000'000;

    meios::detail::eval_session session(collection(bound));
    const ceiling::outcome refused = ceiling::evaluate(separator_only(separators), session);

    CHECK(separators + 1 > 1000 * bound);
    CHECK(refused.refused_on("collection element"));
    CHECK(session.counters.elements == bound);
}

TEST_CASE("the element bound admits a split of exactly its worth and refuses one field more",
          "[native][elements]")
{
    CHECK(once("'a,b,c'.split(',')[2]", collection(3)).rendered == "c");
    CHECK(once("'a,b,c'.split(',')[2]", collection(2)).refused_on("collection element"));
    CHECK(once("'a,b'.split(',')[0]", collection(2)).rendered == "a");
    CHECK(once("'a,b'.split(',')[0]", collection(1)).refused_on("collection element"));
}

// The mapping constructor is the grammar's other collection producer, and it charges through a
// different function, so a bound holding on the split says nothing about it.
TEST_CASE("the mapping constructor charges one element for each keyword entry it takes",
          "[native][elements]")
{
    CHECK(once("dict(a=1, b=2, c=3)['c']", collection(3)).rendered == "3");
    CHECK(once("dict(a=1, b=2, c=3)['c']", collection(2)).refused_on("collection element"));
    CHECK(once("dict(a=1)['a']", collection(1)).rendered == "1");
    CHECK_FALSE(once("dict()", collection(1)).failed);
}

// The two producers share one counter, so a load spends one budget across both rather than a
// fresh one per expression.
TEST_CASE("the element budget accumulates across the expressions of one load",
          "[native][elements]")
{
    meios::detail::eval_session session(collection(4));

    CHECK(ceiling::evaluate("'a,b'.split(',')[0]", session).rendered == "a");
    CHECK(session.counters.elements == 2);
    CHECK(ceiling::evaluate("dict(a=1, b=2)['b']", session).rendered == "2");
    CHECK(session.counters.elements == 4);
    CHECK(ceiling::evaluate("'a,b'.split(',')[0]", session).refused_on("collection element"));
}

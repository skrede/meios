#include "ceiling_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>

namespace
{

// The four axes this stem drives, each set one unit under what its case needs: the numeric
// magnitude, the per-expression token count, the length of a produced string and the element
// count of a produced collection.
meios::evaluator_limits numeric(std::size_t magnitude)
{
    return meios::evaluator_limits{ 0, 0, 0, 0, 0, 0, 0, magnitude };
}

meios::evaluator_limits per_expression(std::size_t tokens)
{
    return meios::evaluator_limits{ 0, 0, 0, 0, 0, 0, 0, 0, tokens };
}

meios::evaluator_limits produced(std::size_t bytes)
{
    return meios::evaluator_limits{ 0, 0, 0, 0, 0, 0, 0, 0, 0, bytes };
}

meios::evaluator_limits collection(std::size_t elements)
{
    return meios::evaluator_limits{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, elements };
}

ceiling::outcome once(std::string_view expression, const meios::evaluator_limits &ceilings)
{
    meios::detail::eval_session session(ceilings);
    return ceiling::evaluate(expression, session);
}

}

TEST_CASE("the numeric ceiling refuses a base or an exponent past it", "[native][ceilings]")
{
    const meios::evaluator_limits bounded = numeric(1000);

    CHECK(once("1 ** 1000", bounded).rendered == "1");
    CHECK(once("1000 ** 1", bounded).rendered == "1000");
    CHECK(once("1 ** 1001", bounded).refused_on("numeric magnitude"));
    CHECK(once("1001 ** 1", bounded).refused_on("numeric magnitude"));
}

// The new check runs before the work; the overflow refusal that already existed runs on the
// result. An exponentiation inside the ceiling must still reach the second one.
TEST_CASE("an exponentiation inside the numeric ceiling still refuses on overflow",
          "[native][ceilings]")
{
    const ceiling::outcome overflowed = once("9 ** 1000", numeric(1000));

    CHECK(overflowed.failed);
    CHECK(overflowed.kind == meios::eval_failure_kind::error);
    REQUIRE(overflowed.messages.size() == 1);
    CHECK(overflowed.messages.front().find("integer power overflow") != std::string::npos);
}

// A magnitude just above the bound would be admitted if the operand were narrowed to a whole
// number before the comparison, so this is what proves the comparison is made on the real.
TEST_CASE("a real magnitude at the numeric bound is admitted and one above it refuses",
          "[native][ceilings]")
{
    const meios::evaluator_limits bounded = numeric(1000);

    CHECK(once("1000.0 ** 1", bounded).rendered == "1000.0");
    CHECK(once("999.5 ** 1", bounded).rendered == "999.5");
    CHECK(once("1000.5 ** 1", bounded).refused_on("numeric magnitude"));
}

TEST_CASE("a wide expression crosses the per-expression token axis alone", "[native][ceilings]")
{
    const meios::evaluator_limits bounded = per_expression(8);
    meios::detail::eval_session session(bounded);

    CHECK(ceiling::evaluate("1+1+1", session).rendered == "3");
    const ceiling::outcome wide = ceiling::evaluate("1+1+1+1+1", session);

    CHECK(wide.refused_on("expression token"));
    CHECK(session.counters.tokens < bounded.tokens);
}

// The other direction of the same distinction: many short expressions exhaust the cumulative
// budget while no single one comes near the per-expression axis.
TEST_CASE("a session of short expressions crosses the cumulative token budget alone",
          "[native][ceilings]")
{
    const meios::evaluator_limits bounded{ 0, 0, 0, 12, 0, 0, 0, 0, 1000 };
    meios::detail::eval_session session(bounded);

    for(int at = 0; at < 3; ++at)
        CHECK(ceiling::evaluate("1 + 1", session).rendered == "2");
    const ceiling::outcome spent = ceiling::evaluate("1 + 1", session);

    CHECK(spent.failed);
    REQUIRE(spent.messages.size() == 1);
    CHECK(spent.messages.front().find("expression token") == std::string::npos);
    CHECK(spent.messages.front().find("token ceiling") != std::string::npos);
    CHECK(session.counters.expression_tokens < bounded.expression_tokens);
}

TEST_CASE("both string producers answer to the same produced-length ceiling",
          "[native][ceilings]")
{
    const meios::evaluator_limits bounded = produced(4);

    CHECK(once("'ab' + 'cd'", bounded).rendered == "abcd");
    CHECK(once("'ab' + 'cde'", bounded).refused_on("string length"));
    CHECK(once("'ab/cd'.split('/')[0]", bounded).rendered == "ab");
    CHECK(once("'abcde/x'.split('/')[0]", bounded).refused_on("string length"));
}

// Measured rather than assumed: an empty expression is not free and is not accepted. It
// tokenizes to the one end token, which is charged, and the grammar then refuses it as a
// construct it does not implement -- so the only zero-length thing the ceilings admit is a
// produced string.
TEST_CASE("a zero-length produced string is admitted and an empty expression is not",
          "[native][ceilings]")
{
    const ceiling::outcome empty_text = once("'' + ''", produced(4));

    CHECK_FALSE(empty_text.failed);
    CHECK(empty_text.rendered.empty());

    const meios::evaluator_limits defaults;
    meios::detail::eval_session session(defaults);
    const ceiling::outcome nothing = ceiling::evaluate("", session);

    CHECK(nothing.failed);
    CHECK(nothing.kind == meios::eval_failure_kind::unsupported);
    CHECK(session.counters.tokens == 1);
}

TEST_CASE("each new ceiling is terminal under every evaluation policy", "[native][ceilings]")
{
    const meios::evaluator_limits magnitude = numeric(1000);
    const meios::evaluator_limits width = per_expression(8);
    const meios::evaluator_limits length = produced(4);
    const meios::evaluator_limits fields = collection(2);

    for(meios::eval_policy policy :
        { meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip })
    {
        meios::detail::eval_session over_magnitude(magnitude);
        meios::detail::eval_session over_width(width);
        meios::detail::eval_session over_length(length);
        meios::detail::eval_session over_fields(fields);

        CHECK_FALSE(ceiling::substitute("v ${1 ** 1001}", policy, over_magnitude).survived);
        CHECK_FALSE(ceiling::substitute("v ${1+1+1+1+1}", policy, over_width).survived);
        CHECK_FALSE(ceiling::substitute("v ${'ab' + 'cde'}", policy, over_length).survived);
        CHECK_FALSE(ceiling::substitute("v ${'a,b,c'.split(',')[0]}", policy, over_fields).survived);
        CHECK(over_magnitude.failure == meios::eval_failure_kind::exhausted);
        CHECK(over_width.failure == meios::eval_failure_kind::exhausted);
        CHECK(over_length.failure == meios::eval_failure_kind::exhausted);
        CHECK(over_fields.failure == meios::eval_failure_kind::exhausted);
    }
}

TEST_CASE("a load that crosses nothing counts only the axes it exercised", "[native][ceilings]")
{
    const meios::evaluator_limits defaults;
    meios::detail::eval_session joined(defaults);
    meios::detail::eval_session raised(defaults);

    CHECK(ceiling::evaluate("'ab' + 'cd'", joined).rendered == "abcd");
    CHECK(joined.counters.string_length == 4);
    CHECK(joined.counters.bytes == 4);
    CHECK(joined.counters.expression_tokens == 4);
    CHECK(joined.counters.numeric_magnitude == 0);
    CHECK(joined.counters.yaml_nodes == 0);

    CHECK(ceiling::evaluate("2 ** 3", raised).rendered == "8");
    CHECK(raised.counters.numeric_magnitude == 3);
    CHECK(raised.counters.string_length == 0);
    CHECK(raised.counters.bytes == 0);
}

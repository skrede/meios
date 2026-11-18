#include "branch_probe.h"
#include "ceiling_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <string_view>

namespace
{

std::string chain(std::string_view link, std::size_t count, std::string_view last)
{
    std::string text;
    for(std::size_t at = 0; at < count; ++at)
        text += link;
    return text + std::string(last);
}

// The pinned counts are those of a hundred-link chain as measured. A cost charged once per link
// doubles with the chain, give or take what its two ends cost; one that re-charges every earlier
// link at each new one grows by four.
void grows_linearly(std::string_view link, std::string_view last, std::size_t measured)
{
    constexpr std::size_t base = 100;
    constexpr std::size_t ends = 64;
    const std::size_t single   = probe::steps_for(chain(link, base, last));
    const std::size_t doubled  = probe::steps_for(chain(link, 2 * base, last));

    INFO("steps at " << base << ": " << single << ", at " << 2 * base << ": " << doubled);
    CHECK(single == measured);
    REQUIRE(single > base);
    CHECK(doubled > single);
    CHECK(doubled <= 2 * single + ends);
}

std::size_t admitted_before_refusal(std::string_view expression, meios::detail::eval_session &session, ceiling::outcome &last)
{
    for(std::size_t admitted = 0; admitted < 1000; ++admitted)
    {
        last = ceiling::evaluate(expression, session);
        if(last.failed)
            return admitted;
    }
    return 1000;
}

}

TEST_CASE("a chained conditional charges steps in proportion to its length", "[native][budget]")
{
    grows_linearly("1 if False else ", "2", 1212);
}

TEST_CASE("a disjunction chain charges steps in proportion to its length", "[native][budget]")
{
    grows_linearly("False or ", "True", 1012);
}

TEST_CASE("a conjunction chain charges steps in proportion to its length", "[native][budget]")
{
    grows_linearly("True and ", "False", 912);
}

TEST_CASE("each read of a wide integer's digits costs one step per nine digits, plus one", "[native][budget]")
{
    const std::string wide(4300, '9');
    const std::size_t remainder = probe::steps_for("7 % 7");

    CHECK(probe::steps_for(wide + " % 7") == remainder + 478);
    CHECK(probe::steps_for(wide + " // " + wide) == probe::steps_for("7 // 7") + 956);
    CHECK(probe::steps_for(wide + " * 0.5") == probe::steps_for("7 * 0.5") + 478);
    CHECK(probe::steps_for("99999999999999999999 % 7") == remainder + 3);
}

TEST_CASE("an operand within 64 bits costs nothing for its digits", "[native][budget]")
{
    CHECK(probe::steps_for("9223372036854775807 % 7") == probe::steps_for("7 % 7"));
}

TEST_CASE("repeated arithmetic on a wide integer crosses the evaluation step ceiling", "[native][budget]")
{
    const meios::evaluator_limits bounded{ 0, 0, 0, 0, 5000 };
    meios::detail::eval_session session(bounded);
    ceiling::outcome last{};

    CHECK(admitted_before_refusal(std::string(4300, '9') + " % 7", session, last) < 11);
    CHECK(last.refused_on("evaluation step"));
}

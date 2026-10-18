#include "branch_probe.h"

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

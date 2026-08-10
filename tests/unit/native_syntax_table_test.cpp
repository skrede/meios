#include "syntax_table.h"
#include "branch_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <iterator>
#include <string_view>

namespace
{
std::vector<std::string_view> every_fragment()
{
    std::vector<std::string_view> all(std::begin(syntax::malformed_fragments),
                                      std::end(syntax::malformed_fragments));
    all.insert(all.end(), std::begin(syntax::wellformed_fragments),
               std::end(syntax::wellformed_fragments));
    return all;
}
}

TEST_CASE("every malformed member of the enumeration refuses", "[native][syntax]")
{
    for(const syntax::carrier &around : syntax::carriers)
        for(std::string_view fragment : syntax::malformed_fragments)
        {
            const std::string text = syntax::formed(around, fragment);
            INFO("expression: " << text);
            const probe::outcome ran = probe::evaluate(text);
            CHECK(ran.failed);
            CHECK(ran.rendered == "None");
            CHECK_FALSE(ran.messages.empty());
        }
}

TEST_CASE("every well-formed member of the enumeration evaluates", "[native][syntax]")
{
    for(const syntax::carrier &around : syntax::carriers)
        for(std::string_view fragment : syntax::wellformed_fragments)
        {
            const std::string text = syntax::formed(around, fragment);
            INFO("expression: " << text);
            const probe::outcome ran = probe::evaluate(text);
            CHECK_FALSE(ran.failed);
            CHECK(ran.messages.empty());
        }
}

// The two carriers of a pair differ only in which way the decision goes, so a verdict that
// differs between them is a verdict taken at run time rather than read off the text. The
// refusal category is compared alongside the boolean because the category is the field an
// evaluation policy reads. No fragment here spells a brace or a square-bracketed literal, so
// this case measures nothing about the constructs the scan leaves to the parser; those are
// measured in the syntax stem.
TEST_CASE("a verdict does not depend on which way the decision goes", "[native][syntax]")
{
    const std::size_t pairs[][2] = { { 0, 1 }, { 2, 4 }, { 3, 5 } };

    for(const std::size_t (&pair)[2] : pairs)
        for(std::string_view fragment : every_fragment())
        {
            const std::string one = syntax::formed(syntax::carriers[pair[0]], fragment);
            const std::string other = syntax::formed(syntax::carriers[pair[1]], fragment);
            const probe::outcome ran_one = probe::evaluate(one);
            const probe::outcome ran_other = probe::evaluate(other);
            INFO("expressions: " << one << " | " << other);
            CHECK(ran_one.failed == ran_other.failed);
            CHECK(ran_one.kind == ran_other.kind);
        }
}

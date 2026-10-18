#include "branch_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>

// Measured on the evaluator as it stood before branch selection moved to a token index, and
// unchanged by that move: they are the baseline a nesting ceiling's headroom is derived from.
namespace
{
constexpr std::size_t nested_parenthesis_steps = 1212;
constexpr std::size_t unary_run_steps          = 112;
constexpr std::size_t plain_arithmetic_steps   = 44;
}

TEST_CASE("a guarded lookup returns its default and reads its key through a whole document", "[native][branch]")
{
    meios::log_sink log;
    const std::string absent  = probe::expanded(probe::guard_document("bare_file"), log);
    const std::string present = probe::expanded(probe::guard_document("full_file"), log);

    REQUIRE_FALSE(absent.empty());
    CHECK(absent.find("radius=\"0.05\"") != std::string::npos);
    REQUIRE_FALSE(present.empty());
    CHECK(present.find("radius=\"0.06\"") != std::string::npos);
}

TEST_CASE("a guarded lookup selects without evaluating the branch it did not take", "[native][branch]")
{
    const probe::outcome absent = probe::evaluate("cfg['radius'] if 'radius' in cfg else 0.05");

    CHECK_FALSE(absent.failed);
    CHECK(absent.rendered == "0.05");
    CHECK(absent.messages.empty());
    CHECK(probe::evaluate("full['radius'] if 'radius' in full else 0.05").rendered == "0.06");
}

TEST_CASE("a guarded division by a zero denominator returns its default", "[native][branch]")
{
    const probe::outcome guarded = probe::evaluate("1/x if x != 0 else 0");

    CHECK_FALSE(guarded.failed);
    CHECK(guarded.rendered == "0");
    CHECK(guarded.messages.empty());
    CHECK(probe::evaluate("1/y if y != 0 else 0").rendered == "0.5");
}

TEST_CASE("the branch not taken names nothing and reports nothing", "[native][branch]")
{
    const probe::outcome ran = probe::evaluate("undefined_name if False else 7");

    CHECK_FALSE(ran.failed);
    CHECK(ran.rendered == "7");
    CHECK(ran.messages.empty());
}

TEST_CASE("the branch not taken reads no auxiliary document", "[native][branch]")
{
    const probe::outcome ran = probe::evaluate("1 if True else xacro.load_yaml(bare_file)['length']");

    CHECK_FALSE(ran.failed);
    CHECK(ran.rendered == "1");
    CHECK(ran.fetches == 0);
    CHECK(probe::evaluate("xacro.load_yaml(bare_file)['length'] if False else 1").fetches == 0);
}

TEST_CASE("a chained conditional selects for every binding of its two conditions", "[native][branch]")
{
    CHECK(probe::evaluate("1 if True else 2 if True else 3").rendered == "1");
    CHECK(probe::evaluate("1 if True else 2 if False else 3").rendered == "1");
    CHECK(probe::evaluate("1 if False else 2 if True else 3").rendered == "2");
    CHECK(probe::evaluate("1 if False else 2 if False else 3").rendered == "3");
}

TEST_CASE("a conditional used as a call argument keeps to its own argument", "[native][branch]")
{
    CHECK(probe::evaluate("max(4 if True else 2, 3)").rendered == "4");
    CHECK(probe::evaluate("max(4 if False else 2, 3)").rendered == "3");
    CHECK(probe::evaluate("max(1, 2 if True else 5)").rendered == "2");
    CHECK(probe::evaluate("max(1, 2 if False else 5)").rendered == "5");
}

TEST_CASE("a keyword inside brackets belongs to the bracketed group", "[native][branch]")
{
    CHECK(probe::evaluate("full['radius'] if True else 0").rendered == "0.06");
    CHECK(probe::evaluate("full['radius'] if False else 0").rendered == "0");
    CHECK(probe::evaluate("full['radius' if True else 'length']").rendered == "0.06");
    CHECK(probe::evaluate("full['radius' if False else 'length']").rendered == "0.12");
}

TEST_CASE("an expression holding no conditional evaluates exactly as it did", "[native][branch]")
{
    CHECK(probe::evaluate("1+2").rendered == "3");
    CHECK(probe::evaluate("(1+2)*3").rendered == "9");
    CHECK(probe::evaluate("-(4-1)").rendered == "-3");
}

TEST_CASE("a malformed conditional still refuses by name", "[native][branch]")
{
    const probe::outcome no_else  = probe::evaluate("1 if True");
    const probe::outcome no_first = probe::evaluate("if True else 1");

    CHECK(no_else.failed);
    REQUIRE_FALSE(no_else.messages.empty());
    CHECK(no_else.messages.front().find("expected 'else' in conditional") != std::string::npos);
    CHECK(no_first.failed);
    REQUIRE_FALSE(no_first.messages.empty());
    CHECK(no_first.messages.front().find("'if'") != std::string::npos);
}

TEST_CASE("a condition whose truth cannot be taken evaluates neither branch", "[native][branch]")
{
    const probe::outcome ran = probe::evaluate("undefined_name if cfg else missing_name");

    CHECK(ran.failed);
    REQUIRE(ran.messages.size() == 1);
    CHECK(ran.messages.front().find("no arithmetic meaning") != std::string::npos);
}
TEST_CASE("an expression holding no branch keyword charges the steps it charged before", "[native][branch]")
{
    CHECK(probe::steps_for(probe::nested(100)) == nested_parenthesis_steps);
    CHECK(probe::steps_for(std::string(100, '-') + "1") == unary_run_steps);
    CHECK(probe::steps_for("1 + 2 * 3 - 4 / 5 + 6 * 7 - 8") == plain_arithmetic_steps);
}

#include "branch_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>

// Measured on the evaluator as it stood before branch selection moved to a token index, and
// unchanged by that move: they are the baseline a nesting ceiling's headroom is derived from.
namespace
{
constexpr std::size_t nested_parenthesis_steps = 1212;
constexpr std::size_t unary_run_steps = 112;
constexpr std::size_t plain_arithmetic_steps = 44;
}

TEST_CASE("a guarded lookup returns its default and reads its key through a whole document",
          "[native][branch]")
{
    meios::log_sink log;
    const std::string absent = probe::expanded(probe::guard_document("bare_file"), log);
    const std::string present = probe::expanded(probe::guard_document("full_file"), log);

    REQUIRE_FALSE(absent.empty());
    CHECK(absent.find("radius=\"0.05\"") != std::string::npos);
    REQUIRE_FALSE(present.empty());
    CHECK(present.find("radius=\"0.06\"") != std::string::npos);
}

TEST_CASE("a guarded lookup selects without evaluating the branch it did not take",
          "[native][branch]")
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

TEST_CASE("a chained conditional selects for every binding of its two conditions",
          "[native][branch]")
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
    const probe::outcome no_else = probe::evaluate("1 if True");
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
    const probe::outcome ran = probe::evaluate("undefined_name if word else missing_name");

    CHECK(ran.failed);
    REQUIRE(ran.messages.size() == 1);
    CHECK(ran.messages.front().find("no arithmetic meaning") != std::string::npos);
}

TEST_CASE("and stops at a false left operand and or stops at a true one", "[native][branch]")
{
    const probe::outcome conjunction = probe::evaluate("x != 0 and 1/x > 1");
    const probe::outcome disjunction = probe::evaluate("x == 0 or 1/x > 1");

    CHECK_FALSE(conjunction.failed);
    CHECK(conjunction.rendered == "False");
    CHECK(conjunction.messages.empty());
    CHECK_FALSE(disjunction.failed);
    CHECK(disjunction.rendered == "True");
    CHECK(disjunction.messages.empty());
    CHECK(probe::evaluate("y != 0 and 1/y > 0.4").rendered == "True");
}

TEST_CASE("the operand a decision made unnecessary is never named", "[native][branch]")
{
    CHECK(probe::evaluate("False and undefined_name").rendered == "False");
    CHECK(probe::evaluate("False and undefined_name").messages.empty());
    CHECK(probe::evaluate("True or undefined_name").rendered == "True");
    CHECK(probe::evaluate("True or undefined_name").messages.empty());
}

TEST_CASE("a chain stops at the first decisive operand and stays stopped", "[native][branch]")
{
    CHECK(probe::evaluate("True and False and undefined_name").rendered == "False");
    CHECK(probe::evaluate("True and False and undefined_name").messages.empty());
    CHECK(probe::evaluate("False or False or 1 < 2").rendered == "True");
}

TEST_CASE("mixed precedence is unchanged by the short-circuit", "[native][branch]")
{
    CHECK(probe::evaluate("False and True or True").rendered == "True");
    CHECK(probe::evaluate("True or False and False").rendered == "True");
    CHECK(probe::evaluate("False or True and False").rendered == "False");
    CHECK(probe::evaluate("True and False or False").rendered == "False");
}

TEST_CASE("a skipped operand skips the whole parenthesised group", "[native][branch]")
{
    CHECK(probe::evaluate("False and (True or undefined_name)").rendered == "False");
    CHECK(probe::evaluate("False and (True or undefined_name)").messages.empty());
    CHECK(probe::evaluate("True or (False and undefined_name)").rendered == "True");
    CHECK(probe::evaluate("True or (False and undefined_name)").messages.empty());
}

TEST_CASE("a skipped operand inside a condition ends at the else", "[native][branch]")
{
    CHECK(probe::evaluate("max(1, 2) if False or True else 0").rendered == "2");
    CHECK(probe::evaluate("7 if True or undefined_name else 0").rendered == "7");
    CHECK(probe::evaluate("7 if False and undefined_name else 9").rendered == "9");
}

TEST_CASE("a skipped operand ends at an argument comma", "[native][branch]")
{
    CHECK(probe::evaluate("max(False and undefined_name, 3)").rendered == "3");
    CHECK(probe::evaluate("max(False and undefined_name, 3)").messages.empty());
    CHECK(probe::evaluate("min(True or undefined_name, -1)").rendered == "-1");
}

TEST_CASE("a string operand still refuses as unsupported and says so once", "[native][branch]")
{
    const probe::outcome ran = probe::evaluate("word and True");

    CHECK(ran.kind == meios::eval_failure_kind::unsupported);
    REQUIRE(ran.messages.size() == 1);
    CHECK(ran.messages.front().find("no arithmetic meaning") != std::string::npos);
    CHECK(probe::evaluate("word or True").kind == meios::eval_failure_kind::unsupported);
}

TEST_CASE("an expression holding no branch keyword charges the steps it charged before",
          "[native][branch]")
{
    CHECK(probe::steps_for(probe::nested(100)) == nested_parenthesis_steps);
    CHECK(probe::steps_for(std::string(100, '-') + "1") == unary_run_steps);
    CHECK(probe::steps_for("1 + 2 * 3 - 4 / 5 + 6 * 7 - 8") == plain_arithmetic_steps);
}

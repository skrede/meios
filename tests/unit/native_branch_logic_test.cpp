#include "branch_probe.h"

#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("a collection operand still refuses and says so once, where a string now decides", "[native][branch]")
{
    const probe::outcome ran = probe::evaluate("cfg and True");

    CHECK(ran.kind == meios::eval_failure_kind::error);
    REQUIRE(ran.messages.size() == 1);
    CHECK(ran.messages.front().find("no arithmetic meaning") != std::string::npos);
    CHECK(probe::evaluate("cfg or True").kind == meios::eval_failure_kind::error);
    CHECK(probe::evaluate("word and True").rendered == "True");
}

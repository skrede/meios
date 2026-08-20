#include "string_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <string_view>

TEST_CASE("two strings add to their concatenation, either of them empty", "[native][string]")
{
    CHECK(strings::evaluate("'x' + 'y'").rendered == "xy");
    CHECK(strings::evaluate("'' + 'y'").rendered == "y");
    CHECK(strings::evaluate("'x' + ''").rendered == "x");
    CHECK(strings::evaluate("'' + ''").rendered.empty());
    CHECK(strings::evaluate("prefix + '_' + config['path']").rendered == "arm_meshes/base.dae");
}

TEST_CASE("a string added to any other kind refuses, naming the kind it met", "[native][string]")
{
    const std::pair<std::string_view, std::string_view> refused[] = {
        { "'x' + 1", "integer" },   { "1 + 'x'", "integer" },  { "'x' + 1.5", "real" },
        { "'x' + True", "boolean" }, { "'x' + table", "mapping" }, { "'x' + fields", "sequence" }
    };

    for(const std::pair<std::string_view, std::string_view> &one : refused)
    {
        INFO("expression: " << one.first);
        const strings::outcome ran = strings::evaluate(std::string(one.first));
        CHECK(ran.failed);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find("concatenates only") != std::string::npos);
        CHECK(ran.messages.front().find(one.second) != std::string::npos);
    }
}

TEST_CASE("every arithmetic operator but addition refuses a string operand", "[native][string]")
{
    for(std::string_view form : { "'x' - 'y'", "'x' * 2", "2 * 'x'", "'x' / 'y'", "'x' // 'y'",
                                  "'x' % 'y'", "'x' ** 'y'", "-prefix" })
    {
        INFO("expression: " << form);
        const strings::outcome ran = strings::evaluate(std::string(form));
        CHECK(ran.failed);
        CHECK(ran.kind == meios::eval_failure_kind::unsupported);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find("no arithmetic meaning") != std::string::npos);
    }
}

TEST_CASE("the empty string is false and every other string is true, in every truth position",
          "[native][string]")
{
    CHECK(strings::evaluate("not ''").rendered == "True");
    CHECK(strings::evaluate("not 'a'").rendered == "False");
    CHECK(strings::evaluate("not blank").rendered == "True");
    CHECK(strings::evaluate("'' and True").rendered == "False");
    CHECK(strings::evaluate("'a' and True").rendered == "True");
    CHECK(strings::evaluate("'' or False").rendered == "False");
    CHECK(strings::evaluate("'a' or False").rendered == "True");
    CHECK(strings::evaluate("'yes' if 'a' else 'no'").rendered == "yes");
    CHECK(strings::evaluate("'yes' if '' else 'no'").rendered == "no");
    CHECK(strings::evaluate("'' if prefix else prefix + '_'").rendered.empty());
    CHECK(strings::evaluate("'' if blank else prefix + '_'").rendered == "arm_");
}

TEST_CASE("a collection still has no truth value and refuses wherever one is taken",
          "[native][string]")
{
    for(std::string_view form : { "not table", "not fields", "table and True", "loaded or False",
                                  "'a' if table else 'b'" })
    {
        INFO("expression: " << form);
        const strings::outcome ran = strings::evaluate(std::string(form));
        CHECK(ran.failed);
        CHECK(ran.kind == meios::eval_failure_kind::error);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find("no arithmetic meaning") != std::string::npos);
    }
}

TEST_CASE("membership against a string haystack tests substring containment", "[native][string]")
{
    CHECK(strings::evaluate("'x' in 'axb'").rendered == "True");
    CHECK(strings::evaluate("'z' in 'abc'").rendered == "False");
    CHECK(strings::evaluate("'' in 'abc'").rendered == "True");
    CHECK(strings::evaluate("'' in ''").rendered == "True");
    CHECK(strings::evaluate("'abc' in 'abc'").rendered == "True");
    CHECK(strings::evaluate("'abcd' in 'abc'").rendered == "False");
    CHECK(strings::evaluate("'meshes' in config['path']").rendered == "True");
}

TEST_CASE("a needle that is not a string refuses, and a mapping haystack is unchanged",
          "[native][string]")
{
    const strings::outcome typed = strings::evaluate("1 in 'abc'");

    CHECK(typed.failed);
    REQUIRE_FALSE(typed.messages.empty());
    CHECK(typed.messages.front().find("only a string is contained in a string")
          != std::string::npos);
    CHECK(strings::evaluate("True in 'abc'").failed);
    CHECK(strings::evaluate("'base' in table").rendered == "True");
    CHECK(strings::evaluate("'other' in table").rendered == "False");
    CHECK(strings::evaluate("'path' in config").rendered == "True");
}

TEST_CASE("a chain of concatenations is charged against the same ceilings every expression is",
          "[native][string]")
{
    CHECK(strings::chain_survives(8, 0, 0));
    CHECK_FALSE(strings::chain_survives(8, 8, 0));
    CHECK_FALSE(strings::chain_survives(8, 0, 16));
    CHECK(strings::chain_survives(1, 0, 0));
}

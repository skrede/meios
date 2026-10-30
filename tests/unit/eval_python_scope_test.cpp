#include "eval_python_scope_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using scope_probe::link;
using scope_probe::load;
using scope_probe::property;

TEST_CASE("an expression evaluates only the properties it reads", "[eval_python][scope]")
{
    const scope_probe::loaded got = load(property("c", "True") + property("b", "${nope}") + link("${'x' if c else b}"));
    REQUIRE(scope_probe::renders(got, "x"));
}

TEST_CASE("a name read inside a lambda reads the property", "[eval_python][scope]")
{
    REQUIRE(scope_probe::renders(load(property("k", "3") + link("l${(lambda: k)()}")), "l3"));
}

TEST_CASE("a name read inside a generator expression reads the property", "[eval_python][scope]")
{
    REQUIRE(scope_probe::renders(load(property("k", "3") + link("l${max(x*k for x in range(3))}")), "l6"));
    REQUIRE(scope_probe::renders(load(property("k", "3") + link("l${sum(x*k for x in range(3))}")), "l9"));
}

TEST_CASE("a name read inside a list comprehension reads the property", "[eval_python][scope]")
{
    REQUIRE(scope_probe::renders(load(property("k", "3") + link("l${[x*k for x in range(3)][2]}")), "l6"));
}

TEST_CASE("a deferred property read by the backend is evaluated in the table that defines it", "[eval_python][scope]")
{
    const std::string macro = "<xacro:macro name=\"m\" params=\"k\">" + link("l${d + 1}") + "</xacro:macro>";
    REQUIRE(scope_probe::renders(load(property("k", "3") + property("d", "${k * 2}") + macro + "<xacro:m k=\"5\"/>"), "l7"));
}

TEST_CASE("a property shadows the mathematics name it is spelled as", "[eval_python][scope]")
{
    const std::string body = property("pi", "3") + property("sin", "5") + link("l${pi + 0}_${sin + 1}");
    REQUIRE(scope_probe::renders(load(body), "l3_6"));
    const include_tree::tree held;
    REQUIRE(scope_probe::renders(scope_probe::load_with(held, body, nullptr), "l3_6"));
}

TEST_CASE("a mathematics name no property shadows still reads its own value", "[eval_python][scope]")
{
    REQUIRE(scope_probe::renders(load(link("l${pi}")), "l3.141592653589793"));
    REQUIRE(scope_probe::renders(load(link("l${sin(0)}")), "l0.0"));
}

TEST_CASE("both spellings of the yaml helper still reach it", "[eval_python][scope]")
{
    for(const char *call : {"xacro.load_yaml('absent.yaml')", "load_yaml('absent.yaml')"})
    {
        INFO(call);
        REQUIRE(scope_probe::refuses(load(link(std::string("l${") + call + "}")), "cannot reach the auxiliary document"));
    }
}

TEST_CASE("a bound property spelled for the formatting method still refuses in a document", "[eval_python][scope]")
{
    REQUIRE(scope_probe::refuses(load(property("format", "stl") + link("${'mesh.' + format}")), "format-traversal"));
}

TEST_CASE("a property whose text fails is not replaced by the mathematics name it shadows", "[eval_python][scope]")
{
    const scope_probe::loaded got = load(property("sin", "${nope}") + link("l${sin(0)}"));
    REQUIRE(scope_probe::refuses_once(got, "name 'nope' is not defined"));
}

TEST_CASE("a name nothing binds raises the interpreter's own undefined name", "[eval_python][scope]")
{
    REQUIRE(scope_probe::refuses_once(load(link("l${nope + 1}")), "name 'nope' is not defined"));
}

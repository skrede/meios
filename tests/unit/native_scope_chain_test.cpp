#include "meios/xacro/scope_chain.h"
#include "meios/xacro/macro_tables.h"
#include "meios/xacro/invocation_frame.h"

#include <meios/xacro/value.h>
#include <meios/xacro/eval_scope.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <optional>
#include <stdexcept>

using meios::detail::chain_access;

namespace
{

std::optional<std::int64_t> read(const meios::eval_scope &scope, const char *name)
{
    std::optional<meios::value> found = scope.lookup(name);
    if(!found)
        return std::nullopt;
    return found->integer();
}

meios::detail::macro_def taking(const char *param)
{
    meios::detail::macro_def def{};
    def.params.emplace_back(param);
    return def;
}

void refuse_inside(meios::eval_scope &scope, meios::detail::macro_chain &macros)
{
    const meios::detail::invocation_frame frame(scope, macros);
    scope.set("inner", meios::value{ std::int64_t{ 7 } });
    macros.define("inner", meios::detail::macro_def{});
    throw std::runtime_error("refused");
}

}

TEST_CASE("a name set after a push is gone after the pop, and the outer binding reads again", "[native][scope]")
{
    meios::eval_scope scope;
    scope.set("x", meios::value{ std::int64_t{ 1 } });
    const meios::detail::table_mark mark = chain_access::mark(scope);
    chain_access::push(scope, chain_access::cursor(scope));
    scope.set("x", meios::value{ std::int64_t{ 2 } });
    scope.set("y", meios::value{ std::int64_t{ 3 } });
    REQUIRE(read(scope, "x") == 2);
    chain_access::pop_to(scope, mark);
    REQUIRE(read(scope, "x") == 1);
    REQUIRE_FALSE(scope.contains("y"));
    REQUIRE_FALSE(scope.lookup("y").has_value());
}

TEST_CASE("a read walks outward from the current table and the innermost binding wins", "[native][scope]")
{
    meios::eval_scope scope;
    scope.set("x", meios::value{ std::int64_t{ 1 } });
    scope.set("outer", meios::value{ std::int64_t{ 4 } });
    chain_access::push(scope, chain_access::cursor(scope));
    chain_access::push(scope, chain_access::cursor(scope));
    scope.set("x", meios::value{ std::int64_t{ 2 } });
    REQUIRE(read(scope, "x") == 2);
    REQUIRE(read(scope, "outer") == 4);
    REQUIRE(scope.contains("outer"));
    REQUIRE_FALSE(scope.contains("absent"));
}

TEST_CASE("an erase removes only the current table's binding", "[native][scope]")
{
    meios::eval_scope scope;
    scope.set("x", meios::value{ std::int64_t{ 1 } });
    chain_access::push(scope, chain_access::cursor(scope));
    scope.set("x", meios::value{ std::int64_t{ 2 } });
    scope.erase("x");
    REQUIRE(read(scope, "x") == 1);
    scope.erase("x");
    REQUIRE(read(scope, "x") == 1);
}

TEST_CASE("a pop truncates every table pushed after the mark and restores the cursor", "[native][scope]")
{
    meios::eval_scope scope;
    const meios::detail::table_mark mark = chain_access::mark(scope);
    const std::size_t call = chain_access::push(scope, chain_access::cursor(scope));
    scope.set("a", meios::value{ std::int64_t{ 1 } });
    chain_access::push(scope, call);
    scope.set("b", meios::value{ std::int64_t{ 2 } });
    chain_access::pop_to(scope, mark);
    REQUIRE(chain_access::cursor(scope) == chain_access::document_table);
    REQUIRE(chain_access::mark(scope).size == mark.size);
    REQUIRE_FALSE(scope.contains("a"));
    REQUIRE_FALSE(scope.contains("b"));
}

TEST_CASE("a definition into a named table lands there and not in the current one", "[native][scope]")
{
    meios::eval_scope scope;
    const meios::detail::table_mark mark = chain_access::mark(scope);
    chain_access::push(scope, chain_access::cursor(scope));
    chain_access::define(scope, chain_access::document_table, "kept", meios::value{ std::int64_t{ 5 } });
    chain_access::pop_to(scope, mark);
    REQUIRE(read(scope, "kept") == 5);
}

TEST_CASE("a frame left by an exception drops its symbol and macro tables", "[native][scope]")
{
    meios::eval_scope scope;
    meios::detail::macro_chain macros;
    const meios::detail::table_mark mark = chain_access::mark(scope);
    REQUIRE_THROWS(refuse_inside(scope, macros));
    REQUIRE(chain_access::mark(scope).size == mark.size);
    REQUIRE(chain_access::cursor(scope) == mark.cursor);
    REQUIRE_FALSE(scope.contains("inner"));
    REQUIRE(macros.mark().size == 1);
    REQUIRE(macros.cursor() == 0);
    REQUIRE_FALSE(macros.find("inner").has_value());
}

TEST_CASE("a macro lookup walks outward from the current table and the innermost definition wins", "[native][scope]")
{
    meios::detail::macro_chain macros;
    macros.define("outer", taking("a"));
    macros.define("shadowed", meios::detail::macro_def{});
    const meios::detail::table_mark mark = macros.mark();
    macros.push(macros.cursor());
    macros.define("shadowed", taking("b"));
    REQUIRE(macros.find("outer")->params == std::vector<std::string>{ "a" });
    REQUIRE(macros.find("shadowed")->params == std::vector<std::string>{ "b" });
    macros.pop_to(mark);
    REQUIRE(macros.find("shadowed")->params.empty());
}

TEST_CASE("a scope moves with its tables", "[native][scope]")
{
    meios::eval_scope scope;
    scope.set("x", meios::value{ std::int64_t{ 1 } });
    meios::eval_scope moved = std::move(scope);
    REQUIRE(read(moved, "x") == 1);
}

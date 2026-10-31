#include "stack_meter.h"
#include "eval_python_scope_fixture.h"

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

using scope_probe::include_as;
using scope_probe::link;
using scope_probe::load;
using scope_probe::property;

namespace
{

// Each case runs the same document under the python backend and under the core evaluator.
bool both_render(const include_tree::tree &held, std::string_view body, std::string_view name)
{
    return scope_probe::renders(scope_probe::load_with(held, body, scope_probe::python()), name) && scope_probe::renders(scope_probe::load_with(held, body, nullptr), name);
}

bool both_refuse_once(const include_tree::tree &held, std::string_view body, std::string_view naming)
{
    return scope_probe::refuses_once(scope_probe::load_with(held, body, scope_probe::python()), naming) &&
            scope_probe::refuses_once(scope_probe::load_with(held, body, nullptr), naming);
}

// Property i reads property i + 1 inside arithmetic, so no level is a bare read the substitution layer
// answers and every one is an evaluation in the interpreter.
std::string arithmetic_chain(std::size_t levels)
{
    std::string body;
    for(std::size_t at = 0; at < levels; ++at)
        body += property("p" + std::to_string(at), "${p" + std::to_string(at + 1) + " + 1}");
    return body + property("p" + std::to_string(levels), "1") + link("${p0}");
}

// Stopped one past the pinned length, so a chain that grew reads one level too long.
std::size_t longest_chain(std::size_t cap)
{
    std::size_t levels = 0;
    while(levels < cap && load(arithmetic_chain(levels + 1)).out.has_value())
        ++levels;
    return levels;
}

}

TEST_CASE("a namespace member is read through a dot", "[eval_python][scope]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "7")));
    REQUIRE(both_render(held, include_as("w.xacro", "n") + link("a${n.w}"), "a7"));
}

TEST_CASE("a namespace member named like a private attribute is read through a dot", "[eval_python][scope]")
{
    const include_tree::tree held;
    held.write("m.xacro", include_tree::robot(property("_member", "m7")));
    REQUIRE(both_render(held, include_as("m.xacro", "n") + link("a${n._member}"), "am7"));
}

TEST_CASE("a member the namespace lacks is read from the table the include was read in", "[eval_python][scope]")
{
    const include_tree::tree held;
    held.write("n.xacro", include_tree::robot(property("y", "ny")));
    REQUIRE(both_render(held, property("x", "px") + include_as("n.xacro", "n") + link("${n.x}"), "px"));
}

TEST_CASE("a deferred namespace member sees a global written after the include", "[eval_python][scope]")
{
    const include_tree::tree held;
    held.write("n.xacro", include_tree::robot(property("lz", "a${g}")));
    REQUIRE(both_render(held, include_as("n.xacro", "n") + property("g", "late") + link("${n.lz}"), "alate"));
}

TEST_CASE("a member is read through two nested namespaces", "[eval_python][scope]")
{
    const include_tree::tree held;
    held.write("b.xacro", include_tree::robot(property("c", "lC")));
    held.write("a.xacro", include_tree::robot(include_as("b.xacro", "b")));
    REQUIRE(both_render(held, include_as("a.xacro", "a") + link("${a.b.c}"), "lC"));
}

TEST_CASE("a member no table binds refuses as an undefined name", "[eval_python][scope]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "7")));
    REQUIRE(both_refuse_once(held, include_as("w.xacro", "n") + link("a${n.nope}"), "name 'nope' is not defined"));
}

TEST_CASE("a mathematics name no table binds is read through a namespace", "[eval_python][scope]")
{
    const include_tree::tree held;
    held.write("c.xacro", include_tree::robot(property("c", "C")));
    REQUIRE(both_render(held, include_as("c.xacro", "n") + link("l${n.pi}"), "l3.141592653589793"));
    REQUIRE(both_render(held, include_as("c.xacro", "n") + link("l${n.n.pi}"), "l3.141592653589793"));
}

TEST_CASE("a function no table binds is called through a namespace", "[eval_python][scope]")
{
    const include_tree::tree held;
    held.write("c.xacro", include_tree::robot(property("c", "C")));
    REQUIRE(both_render(held, include_as("c.xacro", "n") + link("l${n.sin(0)}"), "l0.0"));
    REQUIRE(both_render(held, include_as("c.xacro", "n") + link("l${n.max(1, 2)}"), "l2"));
}

TEST_CASE("a namespace member whose text fails gives one record", "[eval_python][scope]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "${nope}")));
    REQUIRE(both_refuse_once(held, include_as("w.xacro", "n") + link("a${n.w + 1}"), "name 'nope' is not defined"));
}

TEST_CASE("a circular definition read by the backend refuses as circular", "[eval_python][scope]")
{
    const scope_probe::loaded got = load(property("a", "${b + 1}") + property("b", "${a + 1}") + link("l${a + 1}"));
    REQUIRE(scope_probe::refuses_once(got, "circular variable definition"));
    REQUIRE(got.out.error().code == meios::diagnostic_code::xacro_structural_error);
}

// CPython 3.14 refuses this chain with "Parser stack overflowed" at 48 levels, and earlier versions
// have no such guard, so the ceiling's charge on each read is what keeps the thread's headroom.
TEST_CASE("a deferred chain read by the backend refuses on the shared ceiling one level past its pin", "[eval_python][scope]")
{
    const std::size_t most = longest_chain(7);
    CHECK(most == 6);
    const scope_probe::loaded refused = load(arithmetic_chain(most + 1));
    REQUIRE(scope_probe::refuses_once(refused, "expression depth ceiling"));
    REQUIRE(refused.out.error().code == meios::diagnostic_code::expansion_budget_exceeded);
}

// The empty chain runs once unmetered first, so statics it initializes do not count against the
// fixed cost alone.
TEST_CASE("a deferred chain read by the backend keeps ninefold headroom", "[eval_python][scope]")
{
    if(!stack_meter::headroom_measured)
        SKIP("the stack figures belong to an unoptimized, uninstrumented POSIX build");
    constexpr std::size_t thread_stack      = 512 * 1024;
    bool rendered                           = load(arithmetic_chain(0)).out.has_value();
    const std::optional<std::size_t> base   = stack_meter::stack_used(thread_stack, [&] { rendered = load(arithmetic_chain(0)).out.has_value() && rendered; });
    const std::optional<std::size_t> loaded = stack_meter::stack_used(thread_stack, [&] { rendered = load(arithmetic_chain(6)).out.has_value() && rendered; });
    REQUIRE(base);
    REQUIRE(loaded);
    CHECK(rendered);
    INFO("stack used with no level " << *base << " bytes, with the admitted length " << *loaded);
    CHECK(stack_meter::keeps_ninefold(*base, *loaded, thread_stack));
}

TEST_CASE("a dunder traversal from a namespace or from a lambda refuses under the guard", "[eval_python][scope]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "7")));
    for(const char *reach : {"n.__class__", "n.__getattr__.__globals__", "n.__dict__", "(lambda: 0).__globals__"})
    {
        INFO(reach);
        const std::string body = include_as("w.xacro", "n") + link(std::string("a${") + reach + "}");
        REQUIRE(scope_probe::refuses_once(scope_probe::load_with(held, body, scope_probe::python()), "dunder-identifier"));
    }
}

TEST_CASE("a property the backend evaluates is read from inside another backend evaluation", "[eval_python][scope]")
{
    const std::string body = property("k", "3") + property("d", "${sum(x for x in range(k))}") + link("l${d + 1}");
    REQUIRE(scope_probe::renders(load(body), "l4"));
}

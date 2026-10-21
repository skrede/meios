#include "xacro_arg_fixture.h"

#include <meios/io/memory_source.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <functional>

namespace
{

using meios::diagnostic_code;

const std::string head = "<robot name=\"probe\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n";

std::string document(const std::string &body)
{
    return head + body + "</robot>";
}

expansion_result refused(const std::string &body)
{
    recorder log;
    return run(document(body), log);
}

expansion_result expand_with_caller(const std::string &body, const char *caller)
{
    recorder log;
    meios::source_stack sources;
    meios::eval_scope scope;
    scope.bind_argument("a", meios::value{ std::string(caller) });
    meios::log_sink_f sink{ std::ref(log) };
    return meios::expand(document(body), scope, sources, "doc.xacro", meios::expansion_limits{}, sink);
}

expansion_result expand_including(const std::string &body)
{
    recorder log;
    meios::log_sink_f sink{ std::ref(log) };
    meios::memory_source parts{ sink };
    parts.add("pkg", "a.xacro", "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                                "<xacro:arg name=\"a\" default=\"i\"/></robot>");
    meios::source_stack sources{ std::move(parts) };
    meios::eval_scope scope;
    return meios::expand(document(body), scope, sources, "doc.xacro", meios::expansion_limits{}, sink);
}

void require_link(const std::string &rendered, const std::string &name)
{
    CHECK(rendered.find("<link name=\"" + name + "\"") != std::string::npos);
}

void require_refused(const expansion_result &out, diagnostic_code code)
{
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == code);
}

std::string declared(const char *fallback)
{
    return std::string("<xacro:arg name=\"a\" default=\"") + fallback + "\"/>\n";
}

const std::string used = "<link name=\"l_$(arg a)\"/>\n";

const std::string property_x = "<xacro:property name=\"x\" value=\"vx\"/>\n";

}

TEST_CASE("an argument default holding a stray double dollar refuses at its declaration whether or not it is read",
          "[xacro][arg][declaration]")
{
    struct row
    {
        const char *fallback;
        const char *message;
        bool read;
    };
    const row table[] = {
        { "a$$x", "invalid expression: $$x", true },
        { "a$$", "invalid expression: $$", true },
        { "$$", "invalid expression: $$", true },
        { "a$$x", "invalid expression: $$x", false },
    };
    for(const row &r : table)
    {
        INFO("default: " << r.fallback << ", read: " << r.read);
        const expansion_result out = refused(declared(r.fallback) + (r.read ? used : "<link name=\"l\"/>\n"));
        require_refused(out, diagnostic_code::expression_error);
        CHECK(out.error().message == r.message);
        CHECK(out.error().loc.line == 2);
    }
}

TEST_CASE("a caller's argument leaves the declaration's default unread", "[xacro][arg][declaration]")
{
    const expansion_result out = expand_with_caller(declared("a$$x") + used, "foo");
    REQUIRE(out.has_value());
    require_link(out->document, "l_foo");
}

TEST_CASE("a default that lexes to itself is still lent to a use above its declaration",
          "[xacro][arg][declaration]")
{
    require_link(flattened(document(declared("$x") + used)), "l_$x");
    require_link(flattened(document(used + declared("$x"))), "l_$x");
}

TEST_CASE("a use above a default holding a stray double dollar refuses as unset", "[xacro][arg][declaration]")
{
    require_refused(refused(used + declared("a$$x")), diagnostic_code::unresolved_arg);
}

TEST_CASE("a later declaration does not stand in for the first one the expansion reaches",
          "[xacro][arg][declaration]")
{
    const expansion_result run_first = refused(declared("a$$x") + declared("b") + used);
    require_refused(run_first, diagnostic_code::expression_error);
    CHECK(run_first.error().message == "invalid expression: $$x");
    require_refused(refused(declared("${nope}") + declared("b") + used), diagnostic_code::undefined_property);
    require_link(flattened(document(property_x + declared("${x}") + declared("b") + used)), "l_vx");
    require_link(flattened(document(declared("b") + declared("a$$x") + used)), "l_b");
}

TEST_CASE("a declaration the expansion never reaches binds nothing", "[xacro][arg][declaration]")
{
    const std::string skipped = "<xacro:if value=\"false\">" + declared("x") + "</xacro:if>\n";
    const std::string uncalled = "<xacro:macro name=\"m\">" + declared("x") + "</xacro:macro>\n";
    require_link(flattened(document(skipped + declared("y") + used)), "l_y");
    require_link(flattened(document(uncalled + declared("y") + used)), "l_y");
}

TEST_CASE("a declaration in an include reached first binds before the including document's own",
          "[xacro][arg][declaration]")
{
    const expansion_result out =
        expand_including("<xacro:include filename=\"$(find pkg)/a.xacro\"/>\n" + declared("t") + used);
    REQUIRE(out.has_value());
    require_link(out->document, "l_i");
}

TEST_CASE("a declaration in a macro body binds on the first call only", "[xacro][arg][declaration]")
{
    const std::string macro =
        "<xacro:macro name=\"m\" params=\"n\">" + declared("x") + "<link name=\"${n}_$(arg a)\"/></xacro:macro>\n";
    const std::string rendered =
        flattened(document(macro + "<xacro:m n=\"one\"/>\n" + declared("z") + "<xacro:m n=\"two\"/>\n"));
    require_link(rendered, "one_x");
    require_link(rendered, "two_x");
}

TEST_CASE("a reached declaration reads its own default with no lent value bound", "[xacro][arg][declaration]")
{
    require_refused(refused(declared("$(arg a)y") + declared("x") + used), diagnostic_code::unresolved_arg);
    require_link(flattened(document(declared("y") + declared("x") + used)), "l_y");
    require_link(flattened(document(declared("x") + used)), "l_x");
}

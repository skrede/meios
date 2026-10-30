#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <algorithm>

namespace
{

using include_tree::renders_link;
using include_tree::expansion_result;

struct record
{
    meios::level lvl;
    std::optional<meios::diagnostic_code> code;
    std::optional<meios::source_location> where;
    std::string message;
};

struct recorder
{
    std::vector<record> &sink;

    void operator()(meios::level lvl, const std::string &message)
    {
        sink.push_back({lvl, std::nullopt, std::nullopt, message});
    }

    void operator()(meios::level lvl, const meios::source_location &where, const std::string &message)
    {
        sink.push_back({lvl, std::nullopt, where, message});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &where, const std::string &message)
    {
        sink.push_back({lvl, code, where, message});
    }
};

expansion_result expand_heard(const std::string &body, std::vector<record> &records)
{
    meios::log_sink_f log{recorder{records}};
    meios::source_stack sources{};
    return include_tree::expand_text(include_tree::robot(body), "inline.xacro", sources, log);
}

expansion_result expand_tree_heard(const include_tree::tree &held, const std::string &body, std::vector<record> &records)
{
    meios::log_sink_f log{recorder{records}};
    meios::source_stack sources{};
    return include_tree::expand_file(held.write("top.xacro", include_tree::robot(body)), sources, log);
}

std::size_t warnings(const std::vector<record> &records)
{
    return static_cast<std::size_t>(std::ranges::count(records, meios::level::warn, &record::lvl));
}

// The reference's warning, located at the element and carrying no code, so that it does not
// retract a completeness claim.
bool warned_once(const std::vector<record> &records, const std::string &message)
{
    const auto found = std::ranges::find(records, message, &record::message);
    return warnings(records) == 1 && found != records.end() && found->lvl == meios::level::warn && !found->code && found->where && found->where->line >= 1;
}

bool refuses(const expansion_result &out, meios::diagnostic_code code)
{
    return !out.has_value() && out.error().code == code;
}

}

// Upstream warns "unknown attribute(s)" for an attribute a property does not declare.
TEST_CASE("an attribute outside a property's attribute list raises no warning here", "[xacro][property][warning]")
{
    std::vector<record> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="p" value="1" bogus="x"/><link name="l${p}"/>)", heard);
    REQUIRE(renders_link(out, "l1"));
    REQUIRE(warnings(heard) == 0);
}

TEST_CASE("a property named pi shadows the built-in and warns that it redefines a global symbol", "[xacro][property][warning]")
{
    std::vector<record> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="pi" value="3"/><link name="l${pi}"/>)", heard);
    REQUIRE(renders_link(out, "l3"));
    REQUIRE(warned_once(heard, "redefining global symbol: pi"));
}

TEST_CASE("removing a built-in keeps it and warns that a global symbol cannot be removed", "[xacro][property][warning]")
{
    std::vector<record> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="pi" remove="true"/><link name="l${pi}"/>)", heard);
    REQUIRE(renders_link(out, "l3.141592653589793"));
    REQUIRE(warned_once(heard, "Cannot remove global symbol: pi"));
}

TEST_CASE("a property shadowing a built-in and then removed leaves the built-in readable", "[xacro][property][warning]")
{
    std::vector<record> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="pi" value="3"/><xacro:property name="pi" remove="true"/><link name="l${pi}"/>)", heard);
    REQUIRE(renders_link(out, "l3.141592653589793"));
    REQUIRE(warnings(heard) == 2);
}

TEST_CASE("a default for a built-in counts it as defined and binds nothing", "[xacro][property][warning]")
{
    std::vector<record> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="pi" default="3"/><link name="l${pi}"/>)", heard);
    REQUIRE(renders_link(out, "l3.141592653589793"));
    REQUIRE(warnings(heard) == 0);
}

TEST_CASE("a macro parameter named after a built-in warns that it redefines a global symbol", "[xacro][property][warning]")
{
    std::vector<record> heard;
    const expansion_result out = expand_heard(R"(<xacro:macro name="m" params="sin"><link name="l${sin}"/></xacro:macro><xacro:m sin="2"/>)", heard);
    REQUIRE(renders_link(out, "l2"));
    REQUIRE(warned_once(heard, "redefining global symbol: sin"));
}

TEST_CASE("an include namespace named after a built-in warns that it redefines a global symbol", "[xacro][property][warning]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(R"(<xacro:property name="c" value="C"/>)"));
    std::vector<record> heard;
    const expansion_result out = expand_tree_heard(held, R"(<xacro:include filename="w.xacro" ns="math"/><link name="l"/>)", heard);
    REQUIRE(renders_link(out, "l"));
    REQUIRE(warned_once(heard, "redefining global symbol: math"));
}

// Upstream warns "default property value can only be defined on local scope" and binds the default too.
TEST_CASE("a default beside a scope binds and raises no warning here", "[xacro][property][warning]")
{
    std::vector<record> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="p" default="d" scope="global"/><link name="l${p}"/>)", heard);
    REQUIRE(renders_link(out, "ld"));
    REQUIRE(warnings(heard) == 0);
}

// The reference stores a document-level parent-scope property into its table of built-ins, which
// cannot store, after it has evaluated the value; its "no parent scope" warning is never reached.
TEST_CASE("a parent-scope property at document level refuses with its value's own cause first", "[xacro][property][warning]")
{
    std::vector<record> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="p" value="${nope}" scope="parent"/><link name="l"/>)", heard);
    REQUIRE(refuses(out, meios::diagnostic_code::undefined_property));
}

TEST_CASE("a parent-scope property at document level refuses, there being no parent table", "[xacro][property][warning]")
{
    std::vector<record> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="p" value="1" scope="parent"/><link name="l"/>)", heard);
    REQUIRE(refuses(out, meios::diagnostic_code::xacro_structural_error));
    REQUIRE(warnings(heard) == 0);
}

TEST_CASE("a parent-scope property at a namespaced file's top level binds in the including table", "[xacro][property][warning]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(R"(<xacro:property name="p" value="1" scope="parent"/>)"));
    std::vector<record> heard;
    const expansion_result out = expand_tree_heard(held, R"(<xacro:include filename="w.xacro" ns="n"/><link name="l${p}"/>)", heard);
    REQUIRE(renders_link(out, "l1"));
}

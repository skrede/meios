#include "xacro_read_probe.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <algorithm>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_heard(const std::string &body, std::vector<xacro_probe::captured> &records)
{
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string source = R"(<robot name="r" xmlns:xacro="http://www.ros.org/wiki/xacro">)" + body + "</robot>";
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, log);
}

std::size_t warnings(const std::vector<xacro_probe::captured> &records)
{
    return static_cast<std::size_t>(std::ranges::count(records, meios::level::warn, &xacro_probe::captured::lvl));
}

bool renders_link(const expansion_result &out, const std::string &name)
{
    return out.has_value() && out->document.find("<link name=\"" + name + "\"/>") != std::string::npos;
}

}

// Upstream warns "unknown attribute(s)" for an attribute a property does not declare.
TEST_CASE("an attribute outside a property's attribute list raises no warning here", "[xacro][property][warning]")
{
    std::vector<xacro_probe::captured> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="p" value="1" bogus="x"/><link name="l${p}"/>)", heard);
    REQUIRE(renders_link(out, "l1"));
    REQUIRE(warnings(heard) == 0);
}

// Upstream warns "redefining global symbol: pi" and renders l3.
TEST_CASE("a property redefining a built-in symbol raises no warning here and the built-in stands", "[xacro][property][warning]")
{
    std::vector<xacro_probe::captured> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="pi" value="3"/><link name="l${pi}"/>)", heard);
    REQUIRE(renders_link(out, "l3.141592653589793"));
    REQUIRE(warnings(heard) == 0);
}

// Upstream warns "Cannot remove global symbol: pi" and still reads the built-in, rendering l3.141592653589793.
TEST_CASE("removing a built-in symbol raises no warning here, and binds it empty", "[xacro][property][warning]")
{
    std::vector<xacro_probe::captured> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="pi" remove="true"/><link name="l${pi}"/>)", heard);
    REQUIRE(renders_link(out, "l"));
    REQUIRE(warnings(heard) == 0);
}

// Upstream warns "default property value can only be defined on local scope" and binds the default too.
TEST_CASE("a default beside a scope binds and raises no warning here", "[xacro][property][warning]")
{
    std::vector<xacro_probe::captured> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="p" default="d" scope="global"/><link name="l${p}"/>)", heard);
    REQUIRE(renders_link(out, "ld"));
    REQUIRE(warnings(heard) == 0);
}

// Upstream's "no parent scope at global scope" warning is never reached, because a table's parent is
// never None; it refuses this document instead, storing into the plain dictionary of built-ins.
TEST_CASE("a parent-scope property at the top renders here and raises no warning", "[xacro][property][warning]")
{
    std::vector<xacro_probe::captured> heard;
    const expansion_result out = expand_heard(R"(<xacro:property name="p" value="1" scope="parent"/><link name="l"/>)", heard);
    REQUIRE(renders_link(out, "l"));
    REQUIRE(warnings(heard) == 0);
}

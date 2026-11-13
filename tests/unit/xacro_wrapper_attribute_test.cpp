#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/expansion_error.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace
{

using files = std::vector<std::pair<std::string, std::string>>;

struct listener
{
    std::vector<meios::level> &sink;

    void operator()(meios::level lvl, const std::string &)
    {
        sink.push_back(lvl);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &)
    {
        sink.push_back(lvl);
    }
};

include_tree::expansion_result expand_tree(const std::string &top, const files &included, std::vector<meios::level> &records)
{
    const include_tree::tree held;
    for(const auto &[name, text] : included)
        held.write(name, text);
    const std::filesystem::path document = held.write("top.xacro", top);
    meios::log_sink_f log{listener{records}};
    meios::source_stack sources{meios::directory_source{held.root(), log}};
    return include_tree::expand_file(document, sources, log);
}

std::string shape(const std::string &top, const files &included = {})
{
    std::vector<meios::level> records;
    const include_tree::expansion_result out = expand_tree(top, included, records);
    return out ? meios::canonical_xml(out->document) : std::string("REFUSED");
}

meios::expansion_error refusal(const std::string &top, const files &included = {})
{
    std::vector<meios::level> records;
    include_tree::expansion_result out = expand_tree(top, included, records);
    REQUIRE_FALSE(out.has_value());
    return out.error();
}

bool refuses_undefined(const std::string &top, const files &included = {})
{
    return refusal(top, included).code == meios::diagnostic_code::undefined_property;
}

std::string root_with(std::string_view attributes, std::string_view body)
{
    return "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\" " + std::string(attributes) + '>' + std::string(body) + "</robot>";
}

std::string includes_i(std::string_view before = "")
{
    return include_tree::robot(std::string(before) + include_tree::include("i.xacro"));
}

const std::string inserts_one = R"(<xacro:macro name="o" params="*b"><xacro:insert_block name="b"/></xacro:macro>)";
const std::string inserts_all = R"(<xacro:macro name="w" params="**b"><xacro:insert_block name="b"/></xacro:macro>)";

}

TEST_CASE("an included root whose attribute fails to evaluate refuses the load at that root", "[xacro][output]")
{
    const files included{{"i.xacro", "<?xml version=\"1.0\"?>\n" + root_with(R"(name="${nope}")", R"(<link name="i"/>)")}};
    const meios::expansion_error error = refusal(includes_i(), included);
    REQUIRE(error.code == meios::diagnostic_code::undefined_property);
    REQUIRE(error.loc.file.filename() == "i.xacro");
    REQUIRE(error.loc.line == 2);
}

TEST_CASE("an included root's attributes are evaluated before its content, in the including scope", "[xacro][output]")
{
    const files included{{"i.xacro", root_with(R"(name="${p}")", R"(<xacro:property name="p" value="i"/><link name="${p}"/>)")}};
    REQUIRE(refuses_undefined(includes_i(), included));
    const std::string defined = includes_i(R"(<xacro:property name="p" value="t"/>)");
    REQUIRE(shape(defined, included) == R"(<robot name="probe"><link name="i"></link></robot>)");
}

TEST_CASE("an included root still lifts its declarations and removes its xacro attributes unread", "[xacro][output]")
{
    const files included{{"i.xacro", root_with(R"(xmlns:foo="${u}" xacro:foo="${nope}")", R"(<link name="i"/>)")}};
    const std::string top = includes_i(R"(<xacro:property name="u" value="urn:u"/>)");
    REQUIRE(shape(top, included) == R"(<robot name="probe" xmlns:foo="urn:u"><link name="i"></link></robot>)");
}

TEST_CASE("a macro definition's parameter default refuses once per call", "[xacro][output]")
{
    std::vector<meios::level> records;
    const std::string body = R"(<xacro:macro name="m" params="a:=${nope}"><link name="${a}"/></xacro:macro><xacro:m a="l"/>)";
    REQUIRE_FALSE(expand_tree(include_tree::robot(body), {}, records).has_value());
    REQUIRE(std::ranges::count(records, meios::level::error) == 1);
}

TEST_CASE("a macro definition's attributes are evaluated on each call with its parameters bound", "[xacro][output]")
{
    const std::string defines = R"(<xacro:macro name="m" params="p" doc="${nope}"><link name="${p}"/></xacro:macro>)";
    REQUIRE(shape(include_tree::robot(defines + R"(<link name="l"/>)")) == R"(<robot name="probe"><link name="l"></link></robot>)");
    REQUIRE(refuses_undefined(include_tree::robot(defines + R"(<xacro:m p="l"/>)")));
    const std::string reads_p = R"(<xacro:macro name="m" params="p" doc="${p}"><link name="${p}"/></xacro:macro><xacro:m p="l"/>)";
    REQUIRE(shape(include_tree::robot(reads_p)) == R"(<robot name="probe"><link name="l"></link></robot>)");
}

TEST_CASE("a called macro definition's xacro namespace declaration is evaluated", "[xacro][output]")
{
    const std::string defines = R"(<xacro:macro name="m" xmlns:xacro="${nope}"><link name="l"/></xacro:macro>)";
    REQUIRE(refuses_undefined(include_tree::robot(defines + "<xacro:m/>")));
}

TEST_CASE("an included root's xacro namespace declaration is evaluated", "[xacro][output]")
{
    const files included{{"i.xacro", R"(<robot xmlns:xacro="${nope}"><link name="i"/></robot>)"}};
    REQUIRE(refuses_undefined(includes_i(), included));
}

TEST_CASE("an element's or the top root's xacro namespace declaration is evaluated", "[xacro][output]")
{
    REQUIRE(refuses_undefined(include_tree::robot(R"(<link name="l" xmlns:xacro="${nope}"/>)")));
    REQUIRE(refuses_undefined(R"(<robot name="probe" xmlns:xacro="${nope}"><link name="l"/></robot>)"));
}

TEST_CASE("a block's element and a block wrapper evaluate their xacro namespace declaration", "[xacro][output]")
{
    REQUIRE(refuses_undefined(include_tree::robot(inserts_one + R"(<link name="l"><xacro:o><x xmlns:xacro="${nope}"/></xacro:o></link>)")));
    REQUIRE(refuses_undefined(include_tree::robot(inserts_all + R"(<link name="l"><xacro:w><x xmlns:xacro="${nope}"><visual/></x></xacro:w></link>)")));
}

TEST_CASE("the xacro namespace declaration never reaches the output", "[xacro][output]")
{
    const std::string declared = R"(<link name="l" xmlns:xacro="http://www.ros.org/wiki/xacro"/>)";
    REQUIRE(shape(include_tree::robot(declared)) == R"(<robot name="probe"><link name="l"></link></robot>)");
    const std::string included_root = root_with(R"(name="i")", R"(<link name="i"/>)");
    REQUIRE(shape(includes_i(), {{"i.xacro", included_root}}) == R"(<robot name="probe"><link name="i"></link></robot>)");
    REQUIRE(shape(include_tree::robot(R"(<link xacro:foo="${nope}" name="l"/>)")) == R"(<robot name="probe"><link name="l"></link></robot>)");
}

#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <filesystem>
#include <string_view>

namespace
{

struct heard
{
    meios::level lvl;
    int32_t line;
    std::string message;
};

struct listener
{
    std::vector<heard> &sink;

    void operator()(meios::level lvl, const std::string &message)
    {
        sink.push_back({lvl, 0, message});
    }

    void operator()(meios::level lvl, const meios::source_location &at, const std::string &message)
    {
        sink.push_back({lvl, at.line, message});
    }
};

using files = std::vector<std::pair<std::string, std::string>>;

std::string expand_tree(const std::string &top, const files &included, std::vector<heard> &records)
{
    const include_tree::tree held;
    for(const auto &[name, text] : included)
        held.write(name, text);
    const std::filesystem::path document = held.write("top.xacro", top);
    meios::log_sink_f log{listener{records}};
    meios::source_stack sources{meios::directory_source{held.root(), log}};
    const include_tree::expansion_result out = include_tree::expand_file(document, sources, log);
    return out ? out->document : std::string("REFUSED");
}

std::string shape(const std::string &top, const files &included = {})
{
    std::vector<heard> records;
    const std::string out = expand_tree(top, included, records);
    return out == "REFUSED" ? out : meios::canonical_xml(out);
}

std::string root_declaring(std::string_view declarations, std::string_view body)
{
    return "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\" " + std::string(declarations) + '>' + std::string(body) + "</robot>";
}

const std::string defines_m = R"(<xacro:macro name="m" xmlns:foo="urn:f"><visual/></xacro:macro>)";

}

TEST_CASE("an attribute in the xacro namespace leaves the output unread", "[xacro][output]")
{
    std::vector<heard> records;
    const std::string out = expand_tree(include_tree::robot(R"(<link name="l" xacro:foo="${nope}"/>)"), {}, records);
    REQUIRE(meios::canonical_xml(out) == R"(<robot name="probe"><link name="l"></link></robot>)");
    REQUIRE(records.empty());
}

TEST_CASE("an attribute after one in the xacro namespace is still evaluated", "[xacro][output]")
{
    REQUIRE(shape(include_tree::robot(R"(<link name="l" xacro:foo="b" a="${1}"/>)")) == R"(<robot name="probe"><link a="1" name="l"></link></robot>)");
}

TEST_CASE("a block wrapper's attribute in the xacro namespace does not refuse its insertion", "[xacro][output]")
{
    const std::string body = R"(<xacro:macro name="w" params="**b"><xacro:insert_block name="b"/></xacro:macro>)"
                             R"(<link name="l"><xacro:w><x xacro:foo="${nope}"><visual/></x></xacro:w></link>)";
    REQUIRE(shape(include_tree::robot(body)) == R"(<robot name="probe"><link name="l"><visual></visual></link></robot>)");
}

TEST_CASE("character data is copied as written", "[xacro][output]")
{
    std::vector<heard> records;
    const std::string out = expand_tree(include_tree::robot(R"(<xacro:property name="x" value="v"/><link name="l"><![CDATA[${x}]]></link>)"), {}, records);
    REQUIRE(out.find("<![CDATA[${x}]]>") != std::string::npos);
    REQUIRE(shape(include_tree::robot(R"(<link name="l"><![CDATA[${nope} $(arg q)]]></link>)")) == R"(<robot name="probe"><link name="l">${nope} $(arg q)</link></robot>)");
}

TEST_CASE("an included root's namespace declarations land where the include's content does", "[xacro][output]")
{
    const files included{{"i.xacro", root_declaring(R"(xmlns:foo="urn:f")", R"(<link name="i"/>)")}};
    REQUIRE(shape(include_tree::robot(include_tree::include("i.xacro")), included) == R"(<robot name="probe" xmlns:foo="urn:f"><link name="i"></link></robot>)");
}

TEST_CASE("a lifted declaration is evaluated in the including scope", "[xacro][output]")
{
    const files included{{"i.xacro", root_declaring(R"(xmlns:foo="${u}")", R"(<link name="i"/>)")}};
    const std::string top = include_tree::robot(R"(<xacro:property name="u" value="urn:u"/>)" + include_tree::include("i.xacro"));
    REQUIRE(shape(top, included) == R"(<robot name="probe" xmlns:foo="urn:u"><link name="i"></link></robot>)");
}

TEST_CASE("a macro definition's namespace declarations land on the element holding the call", "[xacro][output]")
{
    REQUIRE(shape(include_tree::robot(defines_m + R"(<link name="l"><xacro:m/></link>)")) == R"(<robot name="probe"><link name="l" xmlns:foo="urn:f"><visual></visual></link></robot>)");
}

TEST_CASE("a redeclared prefix keeps the value it holds and warns only when the values differ", "[xacro][output]")
{
    const std::string top   = include_tree::robot(include_tree::include("a.xacro") + include_tree::include("b.xacro"));
    const std::string first = root_declaring(R"(xmlns:foo="urn:f")", R"(<link name="a"/>)");
    std::vector<heard> records;
    const std::string out = expand_tree(top, {{"a.xacro", first}, {"b.xacro", root_declaring(R"(xmlns:foo="urn:g")", "")}}, records);
    REQUIRE(meios::canonical_xml(out) == R"(<robot name="probe" xmlns:foo="urn:f"><link name="a"></link></robot>)");
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].lvl == meios::level::warn);
    REQUIRE(records[0].line == 1);
    for(std::string_view named : {"'foo'", "\"urn:f\"", "\"urn:g\""})
        REQUIRE(records[0].message.find(named) != std::string::npos);
    records.clear();
    expand_tree(top, {{"a.xacro", first}, {"b.xacro", first}}, records);
    REQUIRE(records.empty());
}

TEST_CASE("a nested include's declaration yields to the one its includer's root holds", "[xacro][output]")
{
    const files included{{"r.xacro", root_declaring(R"(xmlns:a="urn:1")", include_tree::include("n.xacro"))},
                         {"n.xacro", root_declaring(R"(xmlns:a="urn:2" xmlns:b="urn:b")", R"(<link name="n"/>)")}};
    const std::string top = include_tree::robot(include_tree::include("r.xacro"));
    REQUIRE(shape(top, included) == R"(<robot name="probe" xmlns:a="urn:1" xmlns:b="urn:b"><link name="n"></link></robot>)");
}

TEST_CASE("a declaration lifted inside a conditional or a block inserted by its content is dropped", "[xacro][output]")
{
    const std::string wrap = R"(<xacro:macro name="w" params="**b"><xacro:insert_block name="b"/></xacro:macro>)";
    const std::string one  = R"(<xacro:macro name="o" params="*b"><xacro:insert_block name="b"/></xacro:macro>)";
    REQUIRE(shape(include_tree::robot(defines_m + R"(<link name="l"><xacro:if value="1"><xacro:m/></xacro:if></link>)")) ==
            R"(<robot name="probe"><link name="l"><visual></visual></link></robot>)");
    REQUIRE(shape(include_tree::robot(defines_m + wrap + R"(<link name="l"><xacro:w><x><xacro:m/></x></xacro:w></link>)")) ==
            R"(<robot name="probe"><link name="l"><visual></visual></link></robot>)");
    REQUIRE(shape(include_tree::robot(defines_m + one + R"(<link name="l"><xacro:o><x><xacro:m/></x></xacro:o></link>)")) ==
            R"(<robot name="probe"><link name="l"><x xmlns:foo="urn:f"><visual></visual></x></link></robot>)");
}

TEST_CASE("the top root's target namespace becomes its default namespace", "[xacro][output]")
{
    const char *declared = R"(<robot name="probe" xmlns:xacro="http://www.ros.org/wiki/xacro" xacro:targetNamespace="http://x"><link name="l"/></robot>)";
    REQUIRE(shape(declared) == R"(<robot name="probe" xmlns="http://x"><link name="l"></link></robot>)");
    const char *evaluated = R"(<robot name="probe" xmlns:xacro="http://www.ros.org/wiki/xacro" xacro:targetNamespace="${'urn:' + 'e'}" xmlns="urn:old"/>)";
    REQUIRE(shape(evaluated) == R"(<robot name="probe" xmlns="urn:e"></robot>)");
    const char *empty = R"(<robot name="probe" xmlns:xacro="http://www.ros.org/wiki/xacro" xacro:targetNamespace="" xmlns="urn:keep"/>)";
    REQUIRE(shape(empty) == R"(<robot name="probe" xmlns="urn:keep"></robot>)");
}

TEST_CASE("an included root's target namespace has no effect", "[xacro][output]")
{
    const files included{{"t.xacro", root_declaring(R"(xacro:targetNamespace="urn:t")", R"(<link name="i"/>)")}};
    REQUIRE(shape(include_tree::robot(include_tree::include("t.xacro")), included) == R"(<robot name="probe"><link name="i"></link></robot>)");
}

#include "xacro_include_tree.h"
#include "xacro_element_construct.h"

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>

using element_construct::render;
using element_construct::refused;

namespace
{

const std::string lifting_m = R"(<xacro:macro name="m" params="" xmlns:a="urn:a"><visual/></xacro:macro>)";
const std::string defines_m = R"(<xacro:macro name="m" params=""><link name="m"/></xacro:macro>)";

}

TEST_CASE("a declaration lifted inside a renamed element lands where its new name puts it", "[xacro][element][namespace]")
{
    REQUIRE(render(lifting_m + R"(<xacro:element xacro:name="link" name="e"><xacro:m/></xacro:element>)")
            == R"(<robot name="p"><link name="e" xmlns:a="urn:a"><visual></visual></link></robot>)");
    REQUIRE(render(lifting_m + R"(<link name="l"><xacro:element xacro:name="xacro:if" value="1"><xacro:m/></xacro:element></link>)")
            == R"(<robot name="p"><link name="l"><visual></visual></link></robot>)");
    REQUIRE(render(lifting_m + R"(<link name="l"><xacro:element xacro:name="xacro:m"/></link>)")
            == R"(<robot name="p"><link name="l" xmlns:a="urn:a"><visual></visual></link></robot>)");
}

TEST_CASE("a renamed construct warns about an attribute it does not read, never about xacro:name", "[xacro][element]")
{
    std::vector<std::string> heard;
    REQUIRE(render(R"(<xacro:element xacro:name="xacro:if" value="1" foo="x"><link name="t"/></xacro:element>)", heard)
            == R"(<robot name="p"><link name="t"></link></robot>)");
    REQUIRE(heard.size() == 1);
    REQUIRE(heard.front().find("'foo'") != std::string::npos);
}

TEST_CASE("an element renamed into an include passes every check an authored include does", "[xacro][element][include]")
{
    const include_tree::tree held;
    const std::filesystem::path target = held.write("elsewhere/a.xacro", include_tree::binds("leaf"));
    std::filesystem::create_directories(held.root() / "doc");
    if(!include_tree::linked(target, held.root() / "doc" / "a.xacro"))
        SKIP("this host cannot create a symlink");
    held.write("doc/b.xacro", include_tree::binds("inside"));
    meios::log_sink silent;
    meios::source_stack sources{meios::directory_source{held.root() / "root", silent}};
    const std::string inside = R"(<xacro:element xacro:name="xacro:include" filename="b.xacro"/><link name="${leaf}"/>)";
    const std::filesystem::path good = held.write("doc/good.xacro", include_tree::robot(inside));
    const include_tree::expansion_result read = include_tree::expand_file(good, sources, silent);
    REQUIRE(read.has_value());
    REQUIRE(meios::canonical_xml(read->document) == R"(<robot name="probe"><link name="inside"></link></robot>)");
    const std::string outside = R"(<xacro:element xacro:name="xacro:include" filename="a.xacro"/>)";
    const std::filesystem::path bad = held.write("doc/bad.xacro", include_tree::robot(outside));
    const include_tree::expansion_result escaped = include_tree::expand_file(bad, sources, silent);
    REQUIRE_FALSE(escaped.has_value());
    REQUIRE(escaped.error().code == meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("xacro:call naming call refuses because call is xacro:call itself", "[xacro][call]")
{
    for(const std::string written : {"call", "${'call'}"})
    {
        CAPTURE(written);
        const std::string message = refused(defines_m + "<xacro:call macro=\"" + written + "\"/>").message;
        CHECK(message.find("is xacro:call itself") != std::string::npos);
        CHECK(message.find("needs a macro attribute") == std::string::npos);
    }
}

TEST_CASE("xacro:element renamed to xacro:element refuses because it renames itself", "[xacro][element]")
{
    for(const std::string written : {"xacro:element", "${'xacro:element'}"})
    {
        CAPTURE(written);
        const std::string message = refused(R"(<link name="l"><xacro:element xacro:name=")" + written + R"("/></link>)").message;
        CHECK(message.find("renames itself") != std::string::npos);
        CHECK(message.find("needs a xacro:name attribute") == std::string::npos);
    }
}

TEST_CASE("xacro:element renamed to xacro:call still refuses for the macro attribute the call lacks", "[xacro][element][call]")
{
    const std::string message = refused(R"(<link name="l"><xacro:element xacro:name="xacro:call"/></link>)").message;
    CHECK(message.find("needs a macro attribute") != std::string::npos);
    REQUIRE(render(defines_m + R"(<xacro:call macro="m"/>)") == R"(<robot name="p"><link name="m"></link></robot>)");
}

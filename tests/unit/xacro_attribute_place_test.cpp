#include "xacro_attribute_place.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>

using attribute_place::heard;
using attribute_place::render;
using attribute_place::refused;
using attribute_place::at_level;
using attribute_place::placed_on_link;
using attribute_place::require_one_trim_warning;

TEST_CASE("xacro:attribute sets an attribute on the element that holds it", "[xacro][attribute]")
{
    std::vector<heard> records;
    REQUIRE(render(R"(<link name="l"><xacro:attribute name="type" value="t"/></link>)", records)
            == R"(<robot name="p"><link name="l" type="t"></link></robot>)");
    REQUIRE(records.empty());
    REQUIRE(render(R"(<link name="l"><visual/><xacro:attribute name="a" value="1"/></link>)")
            == R"(<robot name="p"><link a="1" name="l"><visual></visual></link></robot>)");
}

TEST_CASE("xacro:attribute overwrites a value its element already holds", "[xacro][attribute]")
{
    REQUIRE(render(R"(<link name="l" type="u"><xacro:attribute name="type" value="t"/></link>)")
            == R"(<robot name="p"><link name="l" type="t"></link></robot>)");
    REQUIRE(render(R"(<link name="l"><xacro:attribute name="a" value="1"/><xacro:attribute name="a" value="2"/></link>)")
            == R"(<robot name="p"><link a="2" name="l"></link></robot>)");
}

TEST_CASE("xacro:attribute under the document's root sets it on the root", "[xacro][attribute]")
{
    REQUIRE(render(R"(<xacro:attribute name="a" value="1"/><link name="l"/>)")
            == R"(<robot a="1" name="p"><link name="l"></link></robot>)");
}

TEST_CASE("xacro:attribute evaluates its name and its value as text", "[xacro][attribute]")
{
    REQUIRE(render(R"(<link name="l"><xacro:attribute name="a" value="${1}"/></link>)") == placed_on_link);
    REQUIRE(render(R"(<xacro:property name="n" value="ty"/><link name="l"><xacro:attribute name="${n}pe" value="${'t'}"/></link>)")
            == R"(<robot name="p"><link name="l" type="t"></link></robot>)");
    REQUIRE(render(R"(<link name="l"><xacro:attribute name="a" value="$${x}"/></link>)")
            == R"(<robot name="p"><link a="${x}" name="l"></link></robot>)");
}

TEST_CASE("xacro:attribute refuses a name that is empty or not an XML name", "[xacro][attribute]")
{
    refused(R"(<link name="l"><xacro:attribute name="" value="1"/></link>)");
    const std::vector<std::string> not_names{"a b", " a b ", "1", " ", "\xc2\xa0" "a", "a&gt;&lt;x", "${1}", "${''}"};
    for(const std::string &name : not_names)
    {
        const meios::expansion_error error = refused("<link name=\"l\">\n<xacro:attribute name=\"" + name + "\" value=\"1\"/></link>");
        REQUIRE(error.loc.line == 3);
    }
    REQUIRE(refused(R"(<link name="l"><xacro:attribute name="a b" value="1"/></link>)").message.find("'a b'") != std::string::npos);
    refused(R"(<link name="l"><xacro:attribute name="a"/></link>)");
}

TEST_CASE("xacro:attribute removes the whitespace around its evaluated name and warns", "[xacro][attribute]")
{
    const std::vector<std::pair<std::string, std::string>> padded{
            {" a ", " a "}, {" a", " a"}, {"a ", "a "}, {"&#9;a", R"(\ta)"}, {"a&#13;&#10;", R"(a\r\n)"}, {"${' a'}", " a"}};
    for(const auto &[written, evaluated] : padded)
    {
        CAPTURE(written);
        std::vector<heard> records;
        REQUIRE(render("<link name=\"l\">\n<xacro:attribute name=\"" + written + "\" value=\"1\"/></link>", records) == placed_on_link);
        require_one_trim_warning(records, 3, evaluated, "a");
    }
    std::vector<heard> records;
    REQUIRE(render(R"(<link name="l"><xacro:attribute name="a" value="1"/></link>)", records) == placed_on_link);
    REQUIRE(records.empty());
}

TEST_CASE("xacro:element renamed to xacro:attribute sets the attribute", "[xacro][attribute]")
{
    REQUIRE(render(R"(<link name="l"><xacro:element xacro:name="xacro:attribute" name="a" value="1"/></link>)") == placed_on_link);
}

TEST_CASE("a namespace declaration set by xacro:attribute meets a lifted one", "[xacro][attribute]")
{
    const std::string defines = R"(<xacro:macro name="m" xmlns:foo="urn:m"><visual/></xacro:macro>)";
    std::vector<heard> records;
    REQUIRE(render(defines + R"(<link name="l"><xacro:attribute name="xmlns:foo" value="urn:a"/><xacro:m/></link>)", records)
            == R"(<robot name="p"><link name="l" xmlns:foo="urn:a"><visual></visual></link></robot>)");
    REQUIRE(at_level(records, meios::level::warn).size() == 1);
    REQUIRE(render(defines + R"(<link name="l"><xacro:m/><xacro:attribute name="xmlns:foo" value="urn:a"/></link>)")
            == R"(<robot name="p"><link name="l" xmlns:foo="urn:a"><visual></visual></link></robot>)");
}

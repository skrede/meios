#include "xacro_attribute_place.h"

#include <meios/diagnostic/level.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using attribute_place::heard;
using attribute_place::names;
using attribute_place::render;
using attribute_place::at_level;
using attribute_place::placed_on_link;
using attribute_place::require_one_note;

namespace
{

const std::string takes_whole = R"(<xacro:macro name="m" params="*b"><xacro:insert_block name="b"/></xacro:macro>)";
const std::string takes_content = R"(<xacro:macro name="m" params="**b"><link name="l"><xacro:insert_block name="b"/></link></xacro:macro>)";

}

TEST_CASE("an attribute under a conditional is placed on the element the conditional's content lands in", "[xacro][attribute]")
{
    require_one_note("<link name=\"l\"><xacro:if value=\"true\">\n<xacro:attribute name=\"a\" value=\"1\"/></xacro:if></link>", 3);
    require_one_note("<link name=\"l\"><xacro:if value=\"1\"><xacro:unless value=\"0\">\n<xacro:attribute name=\"a\" value=\"1\"/></xacro:unless></xacro:if></link>", 3);
}

TEST_CASE("an attribute under a macro's body is placed on the element holding the call", "[xacro][attribute]")
{
    require_one_note("<xacro:macro name=\"m\">\n<xacro:attribute name=\"a\" value=\"1\"/></xacro:macro><link name=\"l\"><xacro:m/></link>", 3);
}

TEST_CASE("an attribute under an included root is placed on the element holding the include", "[xacro][attribute]")
{
    require_one_note(R"x(<link name="l"><xacro:include filename="$(find pkg)/i.xacro"/></link>)x", 2, "<xacro:attribute name=\"a\" value=\"1\"/>\n");
}

TEST_CASE("an attribute under a block's wrapper is placed where the block's content is inserted", "[xacro][attribute]")
{
    require_one_note(takes_content + "<xacro:m><b>\n<xacro:attribute name=\"a\" value=\"1\"/></b></xacro:m>", 3);
    require_one_note(takes_content + "<xacro:m><b c=\"2\">\n<xacro:attribute name=\"a\" value=\"1\"/></b></xacro:m>", 3);
    require_one_note(takes_content + "<xacro:m><b><xacro:if value=\"1\">\n<xacro:attribute name=\"a\" value=\"1\"/></xacro:if></b></xacro:m>", 3);
}

TEST_CASE("an attribute under a block element is inserted with it", "[xacro][attribute]")
{
    std::vector<heard> records;
    REQUIRE(render(takes_whole + R"(<xacro:m><link name="l"><xacro:attribute name="a" value="1"/></link></xacro:m>)", records) == placed_on_link);
    REQUIRE(records.empty());
    require_one_note(takes_whole + "<xacro:m><link name=\"l\"><xacro:if value=\"1\">\n<xacro:attribute name=\"a\" value=\"1\"/></xacro:if></link></xacro:m>", 3);
}

TEST_CASE("an attribute under a block property's body is placed where the property is inserted", "[xacro][attribute]")
{
    require_one_note("<xacro:property name=\"p\">\n<xacro:attribute name=\"a\" value=\"1\"/></xacro:property><link name=\"l\"><xacro:insert_block name=\"p\"/></link>", 3);
}

TEST_CASE("an attribute among a call's arguments sets nothing and says so", "[xacro][attribute]")
{
    std::vector<heard> records;
    REQUIRE(render("<xacro:macro name=\"m\"><link name=\"l\"/></xacro:macro><xacro:m>\n<xacro:attribute name=\"a\" value=\"1\"/></xacro:m>", records)
            == R"(<robot name="p"><link name="l"></link></robot>)");
    const std::vector<heard> warnings = at_level(records, meios::level::warn);
    REQUIRE(warnings.size() == 1);
    REQUIRE(warnings[0].line == 3);
    REQUIRE(names(warnings[0], "'a'"));
    REQUIRE(at_level(records, meios::level::info).empty());
}

namespace
{

const std::string inserts_content = R"(<xacro:macro name="outer" params="**b"><link name="l"><xacro:insert_block name="b"/></link></xacro:macro>)";
const std::string wrap_holding_k  = "<wrap>\n<xacro:attribute name=\"k\" value=\"v\"/><visual><geometry><box size=\"1 1 1\"/></geometry></visual></wrap>";
const std::string visual          = R"(<visual><geometry><box size="1 1 1"></box></geometry></visual>)";

}

TEST_CASE("an attribute in a block passed whole into another call's content insertion is not set and is noted", "[xacro][attribute]")
{
    const std::string forwards = R"(<xacro:macro name="inner" params="*a"><xacro:outer><xacro:insert_block name="a"/></xacro:outer></xacro:macro>)";
    std::vector<heard> records;
    REQUIRE(render(inserts_content + forwards + "<xacro:inner>" + wrap_holding_k + "</xacro:inner>", records) == R"(<robot name="p"><link name="l">)" + visual + "</link></robot>");
    const std::vector<heard> notes = at_level(records, meios::level::info);
    REQUIRE(notes.size() == 1);
    REQUIRE(notes[0].line == 3);
    REQUIRE(names(notes[0], "'k'"));
    REQUIRE(records.size() == 1);
}

TEST_CASE("an attribute in a block passed straight to a content insertion or inserted whole keeps its placement", "[xacro][attribute]")
{
    std::vector<heard> straight;
    REQUIRE(render(inserts_content + "<xacro:outer>" + wrap_holding_k + "</xacro:outer>", straight) == R"(<robot name="p"><link k="v" name="l">)" + visual + "</link></robot>");
    const std::vector<heard> notes = at_level(straight, meios::level::info);
    REQUIRE(notes.size() == 1);
    REQUIRE(notes[0].line == 3);
    REQUIRE(names(notes[0], "'k'"));
    REQUIRE(names(notes[0], "<link>"));

    const std::string inserts = R"(<xacro:macro name="inner" params="*a"><link name="l"><xacro:insert_block name="a"/></link></xacro:macro>)";
    std::vector<heard> whole;
    REQUIRE(render(inserts + "<xacro:inner>" + wrap_holding_k + "</xacro:inner>", whole) == R"(<robot name="p"><link name="l"><wrap k="v">)" + visual + "</wrap></link></robot>");
    REQUIRE(whole.empty());
}

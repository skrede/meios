#include "xacro_attribute_place.h"
#include "xacro_element_construct.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using element_construct::refused;
using element_construct::render;
using attribute_place::heard;
using attribute_place::require_one_trim_warning;

namespace
{

const std::string defines_m = R"(<xacro:macro name="m" params=""><link name="m"/></xacro:macro>)";
const std::string defines_ma = R"(<xacro:macro name="m" params="a"><link name="${a}"/></xacro:macro>)";

}

TEST_CASE("xacro:call expands the macro its macro attribute names", "[xacro][call]")
{
    REQUIRE(render(defines_m + R"(<xacro:call macro="m"/>)") == R"(<robot name="p"><link name="m"></link></robot>)");
}

TEST_CASE("xacro:call binds its other attributes, and macro takes no part in the binding", "[xacro][call]")
{
    REQUIRE(render(defines_ma + R"(<xacro:call macro="m" a="1"/>)") == R"(<robot name="p"><link name="1"></link></robot>)");
    REQUIRE(render(R"(<xacro:macro name="m" params="*b"><link name="m"><xacro:insert_block name="b"/></link></xacro:macro>)"
                   R"(<xacro:call macro="m"><visual/></xacro:call>)")
            == R"(<robot name="p"><link name="m"><visual></visual></link></robot>)");
}

TEST_CASE("xacro:call evaluates its macro attribute and refuses a name no macro answers", "[xacro][call]")
{
    REQUIRE(render(defines_m + R"(<xacro:call macro="${'m'}"/>)") == R"(<robot name="p"><link name="m"></link></robot>)");
    const meios::expansion_error nope = refused(defines_m + "\n\n<xacro:call macro=\"nope\"/>");
    REQUIRE(nope.loc.line == 3);
    REQUIRE(nope.message.find("'nope'") != std::string::npos);
    refused(defines_m + R"(<xacro:call macro="${''}"/>)");
    refused(defines_m + R"(<xacro:call macro="xacro:m"/>)");
}

TEST_CASE("xacro:call names only a macro, never another construct", "[xacro][call]")
{
    const meios::expansion_error conditional = refused(R"(<xacro:call macro="if" value="1"><link name="t"/></xacro:call>)");
    REQUIRE(conditional.message.find("'if'") != std::string::npos);
    refused(R"(<xacro:property name="q" value="1"/><xacro:call macro="property" name="r" value="2"/>)");
}

TEST_CASE("xacro:call naming call refuses for the macro attribute it no longer has", "[xacro][call]")
{
    const meios::expansion_error again = refused(defines_m + R"(<xacro:call macro="call"/>)");
    REQUIRE(again.message.find("macro attribute") != std::string::npos);
    REQUIRE(refused(R"(<xacro:call/>)").message.find("macro attribute") != std::string::npos);
    REQUIRE(refused(R"(<xacro:call macro=""/>)").message.find("macro") != std::string::npos);
}

TEST_CASE("xacro:call resolves a dotted name through an include namespace", "[xacro][call]")
{
    REQUIRE(element_construct::render_with_include(R"x(<xacro:include filename="$(find pkg)/m.xacro" ns="n"/><xacro:call macro="n.m"/>)x", defines_m)
            == R"(<robot name="p"><link name="m"></link></robot>)");
}

TEST_CASE("a declaration lifted from a macro xacro:call names lands where a direct call's lands", "[xacro][call]")
{
    REQUIRE(render(R"(<xacro:macro name="m" params="" xmlns:a="urn:a"><visual/></xacro:macro><link name="l"><xacro:call macro="m"/></link>)")
            == R"(<robot name="p"><link name="l" xmlns:a="urn:a"><visual></visual></link></robot>)");
}

TEST_CASE("xacro:element becomes an element of the name xacro:name gives, without that attribute", "[xacro][element]")
{
    REQUIRE(render(R"(<xacro:element xacro:name="link" name="e"/>)") == R"(<robot name="p"><link name="e"></link></robot>)");
    REQUIRE(render(R"(<xacro:property name="v" value="9"/><xacro:element xacro:name="${'li' + 'nk'}" name="${v}" xmlns:b="urn:b">hi</xacro:element>)")
            == R"(<robot name="p"><link name="9" xmlns:b="urn:b">hi</link></robot>)");
}

TEST_CASE("xacro:element renamed into a conditional decides its content", "[xacro][element]")
{
    REQUIRE(render(R"(<xacro:element xacro:name="xacro:if" value="true"><link name="t"/></xacro:element>)") == R"(<robot name="p"><link name="t"></link></robot>)");
    REQUIRE(render(R"(<xacro:element xacro:name="xacro:if" value="false"><link name="t"/></xacro:element>)") == R"(<robot name="p"></robot>)");
    REQUIRE(render(R"(<xacro:element xacro:name="xacro:unless" value="0"><link name="u"/></xacro:element>)") == R"(<robot name="p"><link name="u"></link></robot>)");
}

TEST_CASE("xacro:element renamed into a property, an argument or a definition defines it", "[xacro][element]")
{
    REQUIRE(render(R"(<xacro:element xacro:name="xacro:property" name="q" value="7"/><link name="l${q}"/>)") == R"(<robot name="p"><link name="l7"></link></robot>)");
    REQUIRE(render(R"x(<xacro:element xacro:name="xacro:arg" name="g" default="5"/><link name="l$(arg g)"/>)x") == R"(<robot name="p"><link name="l5"></link></robot>)");
    REQUIRE(render(R"(<xacro:element xacro:name="xacro:macro" name="mm" params=""><link name="mm"/></xacro:element><xacro:mm/>)")
            == R"(<robot name="p"><link name="mm"></link></robot>)");
}

TEST_CASE("xacro:element renamed into a macro call binds everything but xacro:name", "[xacro][element]")
{
    REQUIRE(render(defines_m + R"(<xacro:element xacro:name="xacro:m"/>)") == R"(<robot name="p"><link name="m"></link></robot>)");
    REQUIRE(render(defines_ma + R"(<xacro:element xacro:name="xacro:m" a="z"/>)") == R"(<robot name="p"><link name="z"></link></robot>)");
    REQUIRE(render(defines_ma + R"(<xacro:element xacro:name="xacro:call" macro="m" a="w"/>)") == R"(<robot name="p"><link name="w"></link></robot>)");
    refused(R"(<xacro:element xacro:name="xacro:nope"/>)");
}

TEST_CASE("xacro:element renamed into xacro:element refuses for the xacro:name it no longer has", "[xacro][element]")
{
    REQUIRE(refused(R"(<xacro:element xacro:name="xacro:element"/>)").message.find("xacro:name attribute") != std::string::npos);
    REQUIRE(refused(R"(<xacro:element name="x"/>)").message.find("xacro:name attribute") != std::string::npos);
}

TEST_CASE("xacro:element refuses an evaluated name that is not an XML name, naming the text", "[xacro][element]")
{
    for(const std::string written : {"${1}", "a b", " a b ", "a&gt;&lt;x", "1x", "${''}", " ", "-a", "a\xc3"})
    {
        CAPTURE(written);
        const meios::expansion_error bad = refused("\n<xacro:element xacro:name=\"" + written + "\"/>");
        REQUIRE(bad.loc.line == 2);
    }
    REQUIRE(refused(R"(<xacro:element xacro:name="a&gt;&lt;x"/>)").message.find("'a><x'") != std::string::npos);
    REQUIRE(refused(R"(<xacro:element xacro:name="${1}"/>)").message.find("'1'") != std::string::npos);
}

TEST_CASE("xacro:element removes the whitespace around its evaluated name and warns", "[xacro][element]")
{
    for(const std::string written : {"link ", " link", "link&#9;"})
    {
        CAPTURE(written);
        std::vector<heard> records;
        REQUIRE(attribute_place::render("\n<xacro:element xacro:name=\"" + written + "\" name=\"l\"/>", records)
                == R"(<robot name="p"><link name="l"></link></robot>)");
        require_one_trim_warning(records, 3, written == "link&#9;" ? "link\t" : written, "link");
    }
    std::vector<heard> records;
    REQUIRE(attribute_place::render(R"(<xacro:element xacro:name="xacro:if " value="1"><link name="t"/></xacro:element>)", records)
            == R"(<robot name="p"><link name="t"></link></robot>)");
    require_one_trim_warning(records, 2, "xacro:if ", "xacro:if");
}

TEST_CASE("xacro:element accepts any XML name, a non-ASCII or a prefixed one among them", "[xacro][element]")
{
    REQUIRE(render("<xacro:element xacro:name=\"\xc3\xa9\"/>") == "<robot name=\"p\"><\xc3\xa9></\xc3\xa9></robot>");
    REQUIRE(render(R"(<xacro:element xacro:name="a:b"/>)") == R"(<robot name="p"><a:b></a:b></robot>)");
    REQUIRE(render(R"(<xacro:element xacro:name="_a.b-1"/>)") == R"(<robot name="p"><_a.b-1></_a.b-1></robot>)");
}

TEST_CASE("a renamed element keeps its own line, and its content keeps theirs", "[xacro][element]")
{
    const meios::diagnostic_code undefined = meios::diagnostic_code::undefined_property;
    REQUIRE(refused("\n\n<xacro:element xacro:name=\"link\" name=\"${nope}\"/>", undefined).loc.line == 3);
    REQUIRE(refused("\n<xacro:element xacro:name=\"link\">\n\n<visual name=\"${nope}\"/></xacro:element>", undefined).loc.line == 4);
    REQUIRE(refused("\n<xacro:element xacro:name=\"xacro:if\" value=\"true\">\n<visual name=\"${nope}\"/></xacro:element>", undefined).loc.line == 3);
    REQUIRE(refused("\n<xacro:element xacro:name=\"${nope}\"/>", undefined).loc.line == 2);
}

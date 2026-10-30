#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

const char *head = R"(<robot name="p" xmlns:xacro="http://www.ros.org/wiki/xacro">)";

expansion_result expand_doc(const std::string &body, meios::log_sink &log,
                            meios::expansion_limits limits = meios::expansion_limits{})
{
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(head + body + "</robot>", scope, sources, "inline.xacro", limits, log);
}

std::string flat(const expansion_result &out)
{
    return meios::canonical_xml(out->document);
}

const char *inserts_once = R"(<xacro:macro name="m" params="*b"><xacro:insert_block name="b"/></xacro:macro>)";

const char *inserts_twice =
    R"(<xacro:macro name="m" params="*b"><xacro:insert_block name="b"/><xacro:insert_block name="b"/></xacro:macro>)";

}

TEST_CASE("an escaped span a block's call left as text is evaluated where the block is inserted",
          "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:property name="x" value="X"/>)" + std::string(inserts_once)
                       + R"(<xacro:m><link name="$${x}"/></xacro:m>)",
                   silent);
    REQUIRE(out.has_value());
    REQUIRE(flat(out) == R"(<robot name="p"><link name="X"></link></robot>)");
}

TEST_CASE("a block inserted twice is evaluated at each insertion", "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:property name="x" value="X"/>)" + std::string(inserts_twice)
                       + R"(<xacro:m><link name="$${x}"/></xacro:m>)",
                   silent);
    REQUIRE(out.has_value());
    REQUIRE(flat(out) == R"(<robot name="p"><link name="X"></link><link name="X"></link></robot>)");
}

TEST_CASE("a block content-only insertion evaluates the children again", "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:property name="x" value="X"/>)"
                   R"(<xacro:macro name="m" params="**b"><xacro:insert_block name="b"/></xacro:macro>)"
                   R"(<xacro:m><w><link name="$${x}"/></w></xacro:m>)",
                   silent);
    REQUIRE(out.has_value());
    REQUIRE(flat(out) == R"(<robot name="p"><link name="X"></link></robot>)");
}

TEST_CASE("a block's second evaluation reads the inserting call's names", "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:property name="x" value="G"/>)"
                   R"(<xacro:macro name="m" params="x *b"><xacro:insert_block name="b"/></xacro:macro>)"
                   R"(<xacro:m x="P"><link name="a${x}$${x}"/></xacro:m>)",
                   silent);
    REQUIRE(out.has_value());
    REQUIRE(flat(out) == R"(<robot name="p"><link name="aGP"></link></robot>)");
}

TEST_CASE("a macro called from a body inserts the block its caller was given", "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:macro name="inner"><xacro:insert_block name="b"/></xacro:macro>)"
                   R"(<xacro:macro name="outer" params="*b"><xacro:inner/></xacro:macro>)"
                   R"(<xacro:outer><link name="db"/></xacro:outer>)",
                   silent);
    REQUIRE(out.has_value());
    REQUIRE(flat(out) == R"(<robot name="p"><link name="db"></link></robot>)");
}

TEST_CASE("each insertion of a block is charged its whole subtree of output", "[xacro][block]")
{
    meios::log_sink silent;
    const std::string call = R"(<xacro:m><link name="x"><visual/></link></xacro:m>)";
    const meios::expansion_limits ceiling{ 1'000'000, 5 };
    REQUIRE(expand_doc(inserts_once + call, silent, ceiling).has_value());
    const expansion_result crowded = expand_doc(inserts_twice + call, silent, ceiling);
    REQUIRE_FALSE(crowded.has_value());
    REQUIRE(crowded.error().code == meios::diagnostic_code::expansion_budget_exceeded);
    REQUIRE(expand_doc(inserts_twice + call, silent, meios::expansion_limits{ 1'000'000, 7 }).has_value());
}

TEST_CASE("a refusal in a block's second evaluation is located at the insertion", "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc("\n<xacro:macro name=\"m\" params=\"*b\">\n<xacro:insert_block name=\"b\"/>\n</xacro:macro>\n"
                   "<xacro:m><link name=\"$${nobody_bound}\"/></xacro:m>\n",
                   silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::undefined_property);
    REQUIRE(out.error().loc.file == "inline.xacro");
    REQUIRE(out.error().loc.line == 3);
}

TEST_CASE("an insertion naming no block bound anywhere on the chain refuses", "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:macro name="m" params="*b"><xacro:insert_block name="c"/></xacro:macro>)"
                   R"(<xacro:m><link name="x"/></xacro:m>)",
                   silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message == "Undefined block \"c\"");
}

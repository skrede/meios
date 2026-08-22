#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <utility>
#include <iterator>
#include <filesystem>

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

// The macro is defined in the included file and called from the top document, so a span that
// refuses inside a block names whichever file the expansion charged it to.
expansion_result expand_calling_include(const std::string &body, meios::log_sink &log)
{
    meios::memory_source parts{ log };
    parts.add("pkg", "m.xacro",
              R"(<robot xmlns:xacro="http://www.ros.org/wiki/xacro">)"
              R"(<xacro:macro name="m" params="*b"><xacro:insert_block name="b"/>)"
              R"(</xacro:macro></robot>)");
    meios::source_stack sources{ std::move(parts) };
    meios::eval_scope scope;
    return meios::expand(head + body + "</robot>", scope, sources, "top.xacro",
                         meios::expansion_limits{}, log);
}

// The minimized document the divergence was measured on, read from where the differential left
// it once the two sides stopped differing and the row came out of the manifest.
expansion_result expand_probe(const char *name, meios::log_sink &log)
{
    std::ifstream in(std::filesystem::path{ MEIOS_XACRO_PROBE_DIR } / name, std::ios::binary);
    std::string source;
    source.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, name, meios::expansion_limits{}, log);
}

const char *discarding_taker =
    R"(<xacro:macro name="taker" params="on **body">)"
    R"(<xacro:if value="${on}"><xacro:insert_block name="body"/></xacro:if></xacro:macro>)";

const char *forwarding =
    R"(<xacro:macro name="inner" params="*origin">)"
    R"(<link name="a"><xacro:insert_block name="origin"/></link></xacro:macro>)"
    R"(<xacro:macro name="outer" params="*origin">)"
    R"(<xacro:inner><xacro:insert_block name="origin"/></xacro:inner></xacro:macro>)"
    R"(<xacro:outer><origin xyz="1 2 3"/></xacro:outer>)";

const char *inserts_twice =
    R"(<xacro:macro name="m" params="*b">)"
    R"(<xacro:insert_block name="b"/><xacro:insert_block name="b"/></xacro:macro>)"
    R"(<xacro:m><link name="x"/></xacro:m>)";

const char *inserts_once =
    R"(<xacro:macro name="m" params="*b"><xacro:insert_block name="b"/></xacro:macro>)"
    R"(<xacro:m><link name="x"/></xacro:m>)";

}

TEST_CASE("a block argument naming an unbound property fails the load where the macro discards it",
          "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out = expand_probe("discarded_block.xacro", silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::undefined_property);
    REQUIRE(out.error().message == "name 'undefined_name_nobody_bound' is not defined");
}

TEST_CASE("a property written inside a block the macro never inserts takes effect at the call site",
          "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(discarding_taker
                       + std::string(R"(<xacro:taker on="false"><wrap>)"
                                     R"(<xacro:property name="w" value="7"/></wrap></xacro:taker>)"
                                     R"(<link name="l${w}"/>)"),
                   silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("l7") != std::string::npos);
}

TEST_CASE("a block argument reads the caller's value where the callee declares the same name",
          "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:property name="v" value="caller"/>)"
                   R"(<xacro:macro name="m" params="v *b"><link name="body_${v}"/>)"
                   R"(<xacro:insert_block name="b"/></xacro:macro>)"
                   R"(<xacro:m v="callee"><link name="blk_${v}"/></xacro:m>)",
                   silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("body_callee") != std::string::npos);
    REQUIRE(out->document.find("blk_caller") != std::string::npos);
}

TEST_CASE("a macro forwarding its own block argument renders the caller's element once",
          "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out = expand_doc(forwarding, silent);
    REQUIRE(out.has_value());
    REQUIRE(meios::canonical_xml(out->document)
            == R"(<robot name="p"><link name="a"><origin xyz="1 2 3"></origin></link></robot>)");
}

TEST_CASE("a block argument inserting the parameter it is bound to refuses as an unknown block",
          "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:macro name="m" params="*b"><xacro:insert_block name="b"/></xacro:macro>)"
                   R"(<xacro:m><xacro:insert_block name="b"/></xacro:m>)",
                   silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message.find("unknown block 'b'") != std::string::npos);
}

TEST_CASE("a block parameter selecting the node and one selecting its children stay distinguishable",
          "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:macro name="m" params="*one **two">)"
                   R"(<a><xacro:insert_block name="one"/></a>)"
                   R"(<b><xacro:insert_block name="two"/></b></xacro:macro>)"
                   R"(<xacro:m><outer><inner/></outer><wrap><kid/></wrap></xacro:m>)",
                   silent);
    REQUIRE(out.has_value());
    const std::string flat = meios::canonical_xml(out->document);
    REQUIRE(flat.find("<a><outer><inner></inner></outer></a>") != std::string::npos);
    REQUIRE(flat.find("<b><kid></kid></b>") != std::string::npos);
}

TEST_CASE("a span refusing inside a block is located in the file the caller wrote it in",
          "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_calling_include(R"(<xacro:include filename="$(find pkg)/m.xacro"/>)"
                               R"(<xacro:m><link name="${nobody_bound}"/></xacro:m>)",
                               silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::undefined_property);
    REQUIRE(out.error().loc.file == "top.xacro");
}

TEST_CASE("text a block's first expansion produced is not evaluated again on insertion",
          "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:property name="brace" value="${'$' + '{x}'}"/>)"
                   R"(<xacro:macro name="m" params="*b"><xacro:insert_block name="b"/></xacro:macro>)"
                   R"(<xacro:m><link name="${brace}"/></xacro:m>)",
                   silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("${x}") != std::string::npos);
}

TEST_CASE("a block inserted twice yields two subtrees and is charged for both", "[xacro][block]")
{
    meios::log_sink silent;
    const expansion_result generous = expand_doc(inserts_twice, silent);
    REQUIRE(generous.has_value());
    REQUIRE(meios::canonical_xml(generous->document)
            == R"(<robot name="p"><link name="x"></link><link name="x"></link></robot>)");

    const meios::expansion_limits ceiling{ 1'000'000, 3 };
    REQUIRE(expand_doc(inserts_once, silent, ceiling).has_value());
    const expansion_result crowded = expand_doc(inserts_twice, silent, ceiling);
    REQUIRE_FALSE(crowded.has_value());
    REQUIRE(crowded.error().code == meios::diagnostic_code::expansion_budget_exceeded);
}

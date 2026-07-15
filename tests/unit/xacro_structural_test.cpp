#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

#ifndef MEIOS_GOLDEN_DIR
    #error "MEIOS_GOLDEN_DIR must name the golden-corpus root"
#endif

namespace
{

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string expand_case(const std::filesystem::path &dir, meios::log_sink &log)
{
    meios::directory_source fixture{ dir, log };
    meios::source_stack sources{ std::move(fixture) };
    meios::eval_scope scope;
    std::string source = read_file(dir / "input.xacro");
    meios::expansion out = meios::expand(source, scope, sources, dir / "input.xacro",
                                         meios::expansion_limits{}, log);
    REQUIRE(out.ok);
    return meios::canonical_xml(out.document);
}

meios::expansion expand_source(const std::string &source, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, log);
}

const char *inherit_hit =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:property name=\"a\" value=\"7\"/>"
    "<xacro:macro name=\"m\" params=\"a:=^\"><link name=\"l${a}\"/></xacro:macro>"
    "<xacro:m/></robot>";

const char *inherit_fallback =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"a:=^|5\"><link name=\"l${a}\"/></xacro:macro>"
    "<xacro:m/></robot>";

const char *inherit_missing =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"a:=^\"><link name=\"l${a}\"/></xacro:macro>"
    "<xacro:m/></robot>";

const char *parent_one_level =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:property name=\"p\" value=\"1\"/>"
    "<xacro:macro name=\"inner\">"
    "<xacro:property name=\"p\" value=\"9\" scope=\"parent\"/></xacro:macro>"
    "<xacro:macro name=\"outer\"><xacro:inner/><link name=\"in${p}\"/></xacro:macro>"
    "<xacro:outer/><link name=\"top${p}\"/></robot>";

const char *parent_at_top =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"setter\">"
    "<xacro:property name=\"q\" value=\"5\" scope=\"parent\"/></xacro:macro>"
    "<xacro:setter/><link name=\"g${q}\"/></robot>";

const char *default_local =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"d\"><xacro:property name=\"z\" value=\"3\"/></xacro:macro>"
    "<xacro:d/><link name=\"d${z}\"/></robot>";

const char *nested_default_read =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"outer\">"
    "<xacro:property name=\"sysval\" value=\"3\"/>"
    "<xacro:macro name=\"inner\"><link name=\"j${sysval}\"/></xacro:macro>"
    "<xacro:inner/></xacro:macro>"
    "<xacro:outer/></robot>";

const char *nested_default_no_leak =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"outer\">"
    "<xacro:property name=\"sysval\" value=\"3\"/>"
    "<xacro:macro name=\"inner\"><link name=\"j${sysval}\"/></xacro:macro>"
    "<xacro:inner/></xacro:macro>"
    "<xacro:outer/><link name=\"doc${sysval}\"/></robot>";

const char *param_default_collision =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:property name=\"X\" value=\"doc\"/>"
    "<xacro:macro name=\"B\" params=\"X:=bparam\">"
    "<xacro:property name=\"X\" value=\"99\"/></xacro:macro>"
    "<xacro:macro name=\"A\"><xacro:B/></xacro:macro>"
    "<xacro:A/><link name=\"l${X}\"/></robot>";

const char *global_reaches_doc =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"g\">"
    "<xacro:property name=\"w\" value=\"7\" scope=\"global\"/></xacro:macro>"
    "<xacro:g/><link name=\"gl${w}\"/></robot>";

const char *top_level_default =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:property name=\"t\" value=\"4\"/>"
    "<link name=\"tl${t}\"/></robot>";

const char *param_parent_collision =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:property name=\"X\" value=\"doc\"/>"
    "<xacro:macro name=\"B\" params=\"X:=bparam\">"
    "<xacro:property name=\"X\" value=\"99\" scope=\"parent\"/></xacro:macro>"
    "<xacro:macro name=\"A\"><xacro:B/></xacro:macro>"
    "<xacro:A/><link name=\"l${X}\"/></robot>";

const char *parent_publish =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"pub\" params=\"src:=abc\">"
    "<xacro:property name=\"dst\" value=\"${src}\" scope=\"parent\"/></xacro:macro>"
    "<xacro:macro name=\"wrap\"><xacro:pub/><link name=\"w${dst}\"/></xacro:macro>"
    "<xacro:wrap/></robot>";

const char *include_property_fixture =
    "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:property name=\"p\" value=\"42\"/></robot>";

const char *include_in_macro =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\">"
    "<xacro:include filename=\"$(find pkg)/inc.xacro\"/></xacro:macro>"
    "<xacro:m/><link name=\"x${p}\"/></robot>";

const char *include_at_top =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:include filename=\"$(find pkg)/inc.xacro\"/>"
    "<link name=\"x${p}\"/></robot>";

meios::expansion expand_with_include(const char *source, meios::log_sink &log)
{
    meios::memory_source parts;
    parts.add("pkg", "inc.xacro", include_property_fixture);
    meios::source_stack sources{ std::move(parts) };
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, "top.xacro", meios::expansion_limits{}, log);
}

}

TEST_CASE("every structural golden expands to its expected URDF", "[xacro][structural][golden]")
{
    const std::filesystem::path root = std::filesystem::path(MEIOS_GOLDEN_DIR) / "structural";
    meios::log_sink silent;
    int cases = 0;
    for(const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(root))
    {
        if(!entry.is_directory() || !std::filesystem::exists(entry.path() / "input.xacro"))
            continue;
        INFO("case: " << entry.path().filename().string());
        std::string expected = meios::canonical_xml(read_file(entry.path() / "expected.urdf"));
        REQUIRE(expand_case(entry.path(), silent) == expected);
        ++cases;
    }
    REQUIRE(cases == 3);
}

TEST_CASE("a caret macro default inherits the enclosing binding", "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(inherit_hit, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("l7") != std::string::npos);
}

TEST_CASE("a caret-fallback macro default resolves the fallback when nothing is inherited",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(inherit_fallback, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("l5") != std::string::npos);
}

TEST_CASE("a caret macro default with no inherited value and no fallback fails loudly",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(inherit_missing, silent);
    REQUIRE_FALSE(out.ok);
    REQUIRE(out.document.find("^") == std::string::npos);
}

TEST_CASE("a scope=parent property reaches the immediate caller but not the grandparent",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(parent_one_level, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("in9") != std::string::npos);
    REQUIRE(out.document.find("top1") != std::string::npos);
}

TEST_CASE("a scope=parent property set at top level persists into the document scope",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(parent_at_top, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("g5") != std::string::npos);
}

TEST_CASE("a default-scope property set inside a macro is macro-local and invisible after return",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(default_local, silent);
    REQUIRE_FALSE(out.ok);
    REQUIRE(out.document.find("d3") == std::string::npos);
}

TEST_CASE("a default-scope property is visible to a nested macro while the definer is on the stack",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(nested_default_read, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("j3") != std::string::npos);
}

TEST_CASE("a default-scope property read from the document after the outer macro returns fails",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(nested_default_no_leak, silent);
    REQUIRE_FALSE(out.ok);
    REQUIRE(out.document.find("doc3") == std::string::npos);
}

TEST_CASE("a default-scope write to a name that is also the writer's parameter restores the outer "
          "document value on exit",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(param_default_collision, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("ldoc") != std::string::npos);
    REQUIRE(out.document.find("lbparam") == std::string::npos);
    REQUIRE(out.document.find("l99") == std::string::npos);
}

TEST_CASE("a scope=global property set inside a macro reaches the document scope",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(global_reaches_doc, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("gl7") != std::string::npos);
}

TEST_CASE("a default-scope property defined at document top level persists for a later read",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(top_level_default, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("tl4") != std::string::npos);
}

TEST_CASE("a scope=parent write to a name that is also the writer's parameter never leaks "
          "the private parameter value into the document scope",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(param_parent_collision, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("ldoc") != std::string::npos);
    REQUIRE(out.document.find("lbparam") == std::string::npos);
    REQUIRE(out.document.find("l99") == std::string::npos);
}

TEST_CASE("a scope=parent write of a parameter value under a distinct name reaches the caller",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(parent_publish, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("wabc") != std::string::npos);
}

TEST_CASE("an included top-level property is macro-local when the include is nested in a macro body",
          "[xacro][structural][property][include]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_with_include(include_in_macro, silent);
    REQUIRE_FALSE(out.ok);
    REQUIRE(out.document.find("x42") == std::string::npos);
}

TEST_CASE("an included top-level property persists when the include sits at the document top level",
          "[xacro][structural][property][include]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_with_include(include_at_top, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("x42") != std::string::npos);
}

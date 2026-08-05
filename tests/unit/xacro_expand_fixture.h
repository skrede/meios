#ifndef HPP_GUARD_MEIOS_UNIT_XACRO_EXPAND_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_XACRO_EXPAND_FIXTURE_H

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

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

std::string expand_case(const std::filesystem::path &dir, meios::log_sink &log)
{
    meios::directory_source fixture{ dir, log };
    meios::source_stack sources{ std::move(fixture) };
    meios::eval_scope scope;
    std::string source = read_file(dir / "input.xacro");
    const expansion_result out = meios::expand(source, scope, sources, dir / "input.xacro",
                                               meios::expansion_limits{}, log);
    REQUIRE(out.has_value());
    return meios::canonical_xml(out->document);
}

expansion_result expand_source(const std::string &source, meios::log_sink &log)
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

expansion_result expand_with_include(const char *source, meios::log_sink &log)
{
    meios::memory_source parts{ log };
    parts.add("pkg", "inc.xacro", include_property_fixture);
    meios::source_stack sources{ std::move(parts) };
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, "top.xacro", meios::expansion_limits{}, log);
}

}

#endif

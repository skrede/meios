#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <functional>
#include <string_view>

namespace
{

struct recorder
{
    int errors{ 0 };
    std::string last{};

    void note(meios::level severity, const std::string &text)
    {
        last = text;
        if(severity == meios::level::error)
            ++errors;
    }

    void operator()(meios::level severity, const std::string &text) { note(severity, text); }

    void operator()(meios::level severity, const meios::source_location &, const std::string &text)
    {
        note(severity, text);
    }
};

meios::expansion run(std::string_view source, recorder &log)
{
    meios::source_stack sources;
    meios::eval_scope scope;
    meios::log_sink_f sink{ std::ref(log) };
    return meios::expand(source, scope, sources, "doc.xacro", meios::expansion_limits{}, sink);
}

std::string flattened(std::string_view source)
{
    recorder log;
    const meios::expansion out = run(source, log);
    REQUIRE(out.ok);
    REQUIRE(log.errors == 0);
    return meios::canonical_xml(out.document);
}

}

TEST_CASE("a declaration seen before its use resolves the default", "[xacro][arg]")
{
    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot xmlns:xacro="http://wiki.ros.org/xacro" name="fixed">
  <xacro:arg name="n" default="dval"/>
  <link name="$(arg n)"/>
</robot>)XML";
    const std::string expected = meios::canonical_xml(
        R"XML(<robot name="fixed"><link name="dval"/></robot>)XML");
    REQUIRE(flattened(doc) == expected);
}

TEST_CASE("a use above its own declaration still resolves the default", "[xacro][arg]")
{
    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot xmlns:xacro="http://wiki.ros.org/xacro" name="$(arg n)">
  <xacro:arg name="n" default="dval"/>
  <link name="root"/>
</robot>)XML";
    const std::string expected = meios::canonical_xml(
        R"XML(<robot name="dval"><link name="root"/></robot>)XML");
    REQUIRE(flattened(doc) == expected);
}

TEST_CASE("an arg-free property/macro document expands unchanged", "[xacro][arg]")
{
    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot xmlns:xacro="http://wiki.ros.org/xacro" name="r">
  <xacro:property name="w" value="0.5"/>
  <xacro:macro name="leg" params="side">
    <link name="${side}_leg" size="${w}"/>
  </xacro:macro>
  <xacro:leg side="left"/>
</robot>)XML";
    const std::string expected = meios::canonical_xml(
        R"XML(<robot name="r"><link name="left_leg" size="0.5"/></robot>)XML");
    REQUIRE(flattened(doc) == expected);
}

TEST_CASE("an undeclared arg with no default still loud-fails", "[xacro][arg]")
{
    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot xmlns:xacro="http://wiki.ros.org/xacro" name="$(arg missing)">
  <link name="root"/>
</robot>)XML";
    recorder log;
    const meios::expansion out = run(doc, log);
    REQUIRE_FALSE(out.ok);
    REQUIRE(log.errors >= 1);
}

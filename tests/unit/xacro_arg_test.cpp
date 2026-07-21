#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <functional>
#include <filesystem>
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

    void operator()(meios::level severity, meios::diagnostic_code, const meios::source_location &,
                    const std::string &text)
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

TEST_CASE("a caller override beats the declared default while others keep theirs", "[xacro][arg]")
{
    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot xmlns:xacro="http://wiki.ros.org/xacro" name="r">
  <xacro:arg name="a" default="da"/>
  <xacro:arg name="b" default="db"/>
  <link name="$(arg a)_$(arg b)"/>
</robot>)XML";
    recorder log;
    meios::source_stack sources;
    meios::eval_scope scope;
    scope.set("a", meios::binding{ std::string("override") });
    meios::log_sink_f sink{ std::ref(log) };
    const meios::expansion out =
        meios::expand(doc, scope, sources, "doc.xacro", meios::expansion_limits{}, sink);
    REQUIRE(out.ok);
    REQUIRE(log.errors == 0);
    REQUIRE(meios::canonical_xml(out.document) ==
            meios::canonical_xml(R"XML(<robot name="r"><link name="override_db"/></robot>)XML"));
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

TEST_CASE("a nested arg default resolves at declaration", "[xacro][arg]")
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "meios_arg_nested";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "pkg");

    recorder log;
    meios::log_sink_f sink{ std::ref(log) };
    meios::directory_source on_disk{ root, sink };
    meios::source_stack sources{ std::move(on_disk) };

    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot xmlns:xacro="http://wiki.ros.org/xacro" name="r">
  <xacro:arg name="sub" default="cfg"/>
  <xacro:arg name="path" default="$(find pkg)/config/$(arg sub)/f.yaml"/>
  <link name="$(arg path)"/>
</robot>)XML";

    meios::eval_scope scope;
    const meios::expansion out =
        meios::expand(doc, scope, sources, "doc.xacro", meios::expansion_limits{}, sink);
    const std::string located =
        std::filesystem::weakly_canonical(root / "pkg").string() + "/config/cfg/f.yaml";
    std::filesystem::remove_all(root);

    REQUIRE(out.ok);
    REQUIRE(log.errors == 0);
    REQUIRE(out.document.find(located) != std::string::npos);
}

TEST_CASE("a caller override wins over a resolvable nested default", "[xacro][arg]")
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "meios_arg_nested_override";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "pkg");

    recorder log;
    meios::log_sink_f sink{ std::ref(log) };
    meios::directory_source on_disk{ root, sink };
    meios::source_stack sources{ std::move(on_disk) };

    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot xmlns:xacro="http://wiki.ros.org/xacro" name="r">
  <xacro:arg name="path" default="$(find pkg)/config/f.yaml"/>
  <link name="$(arg path)"/>
</robot>)XML";

    meios::eval_scope scope;
    scope.set("path", meios::binding{ std::string("/override/f.yaml") });
    const meios::expansion out =
        meios::expand(doc, scope, sources, "doc.xacro", meios::expansion_limits{}, sink);
    std::filesystem::remove_all(root);

    REQUIRE(out.ok);
    REQUIRE(log.errors == 0);
    REQUIRE(meios::canonical_xml(out.document) ==
            meios::canonical_xml(R"XML(<robot name="r"><link name="/override/f.yaml"/></robot>)XML"));
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

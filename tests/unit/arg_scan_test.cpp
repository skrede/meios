#include <meios/xacro/arg_scan.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>
#include <string_view>

namespace
{

const meios::arg_declaration *find(const std::vector<meios::arg_declaration> &args,
                                   std::string_view name)
{
    for(const meios::arg_declaration &arg : args)
        if(arg.name == name)
            return &arg;
    return nullptr;
}

}

TEST_CASE("scan_args enumerates declared and referenced args", "[xacro][arg_scan]")
{
    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot name="r" xmlns:xacro="http://ros.org/wiki/xacro">
  <xacro:arg name="a" default="1"/>
  <link name="$(arg b)"/>
</robot>)XML";
    meios::log_sink silent;
    const std::vector<meios::arg_declaration> args = meios::scan_args(doc, silent);

    const meios::arg_declaration *a = find(args, "a");
    const meios::arg_declaration *b = find(args, "b");
    REQUIRE(a != nullptr);
    REQUIRE(a->default_value == std::optional<std::string>("1"));
    REQUIRE(b != nullptr);
    REQUIRE_FALSE(b->default_value.has_value());
}

TEST_CASE("scan_args keeps a declared default when a reference precedes the declaration",
          "[xacro][arg_scan]")
{
    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot name="r" xmlns:xacro="http://ros.org/wiki/xacro">
  <link name="$(arg foo)"/>
  <xacro:arg name="foo" default="bar"/>
</robot>)XML";
    meios::log_sink silent;
    const std::vector<meios::arg_declaration> args = meios::scan_args(doc, silent);

    int count = 0;
    for(const meios::arg_declaration &arg : args)
        if(arg.name == "foo")
            ++count;
    REQUIRE(count == 1);

    const meios::arg_declaration *foo = find(args, "foo");
    REQUIRE(foo != nullptr);
    REQUIRE(foo->default_value == std::optional<std::string>("bar"));
}

TEST_CASE("scan_args fabricates no default for a pure reference", "[xacro][arg_scan]")
{
    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot name="r" xmlns:xacro="http://ros.org/wiki/xacro">
  <link name="$(arg lonely)"/>
</robot>)XML";
    meios::log_sink silent;
    const std::vector<meios::arg_declaration> args = meios::scan_args(doc, silent);

    const meios::arg_declaration *lonely = find(args, "lonely");
    REQUIRE(lonely != nullptr);
    REQUIRE_FALSE(lonely->default_value.has_value());
}

TEST_CASE("scan_args de-duplicates and preserves document order", "[xacro][arg_scan]")
{
    const std::string_view doc =
        R"XML(<?xml version="1.0"?>
<robot name="r" xmlns:xacro="http://ros.org/wiki/xacro">
  <xacro:arg name="a" default="1"/>
  <link name="$(arg a)"/>
  <joint name="$(arg c)"/>
</robot>)XML";
    meios::log_sink silent;
    const std::vector<meios::arg_declaration> args = meios::scan_args(doc, silent);

    REQUIRE(args.size() == 2);
    REQUIRE(args[0].name == "a");
    REQUIRE(args[0].default_value == std::optional<std::string>("1"));
    REQUIRE(args[1].name == "c");
}

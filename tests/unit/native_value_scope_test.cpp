#include "native_value_probe.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <optional>
#include <functional>
#include <string_view>

using namespace native_value_test;

namespace
{

struct heard
{
    meios::diagnostic_code code;
    meios::source_location loc;
    std::string message;
};

struct tally
{
    std::vector<heard> errors;

    void operator()(meios::level, const std::string &)
    {
    }

    void operator()(meios::level, const meios::source_location &, const std::string &)
    {
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc, const std::string &message)
    {
        if(lvl == meios::level::error)
            errors.push_back({code, loc, message});
    }
};

// Every route a value can travel into a scope binding: a property, a macro default, a call-site
// argument, and a property a macro body defines locally before republishing it.
std::string carried_through_every_route()
{
    return document("<xacro:property name=\"copy\" value=\"${x}\"/>"
                    "<xacro:macro name=\"m\" params=\"d:=${x} a\">"
                    "<xacro:property name=\"inner\" value=\"${d}\"/>"
                    "<xacro:property name=\"from_default\" value=\"${d}\" scope=\"global\"/>"
                    "<xacro:property name=\"from_argument\" value=\"${a}\" scope=\"global\"/>"
                    "<xacro:property name=\"from_inner\" value=\"${inner}\" scope=\"global\"/>"
                    "</xacro:macro><xacro:m a=\"${x}\"/>");
}

}

TEST_CASE("a value survives a copy, a move and a scope round trip", "[native][value]")
{
    const meios::value origin = sample_mapping().with_yaml_origin();
    REQUIRE(origin.from_yaml());

    meios::value copied      = origin;
    const meios::value moved = std::move(copied);
    REQUIRE(moved == origin);
    REQUIRE(moved.from_yaml());

    meios::eval_scope scope;
    scope.set("config", moved);
    const std::optional<meios::value> read_back = scope.lookup("config");
    REQUIRE(read_back.has_value());
    REQUIRE(*read_back == origin);
    REQUIRE(read_back->from_yaml());

    const meios::value plain = sample_mapping();
    REQUIRE_FALSE(plain.from_yaml());
    REQUIRE_FALSE(plain == origin);
}

TEST_CASE("a mapping crosses a property, a macro and a nested scope intact", "[native][value]")
{
    meios::eval_scope scope;
    scope.set("config", sample_mapping().with_yaml_origin());
    meios::source_stack sources;
    meios::log_sink silent;

    const std::string source                                                 = document("<xacro:property name=\"copy\" value=\"${config}\"/>"
                                                                                        "<xacro:macro name=\"m\" params=\"table:=${config}\">"
                                                                                        "<xacro:property name=\"inner\" value=\"${table}\" scope=\"global\"/>"
                                                                                        "</xacro:macro><xacro:m/>");
    const meios::expected<meios::expansion, meios::expansion_error> expanded = meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, silent);

    REQUIRE(expanded.has_value());
    REQUIRE(scope.lookup("copy") == std::optional<meios::value>(sample_mapping().with_yaml_origin()));
    REQUIRE(scope.lookup("inner") == std::optional<meios::value>(sample_mapping().with_yaml_origin()));
    REQUIRE(scope.lookup("copy")->from_yaml());
}

TEST_CASE("an exact substitution keeps the evaluated type where mixed text renders it", "[native][value]")
{
    meios::eval_scope scope;
    meios::source_stack sources;
    meios::log_sink silent;

    const std::string source = document("<xacro:property name=\"exact\" value=\"${1+1}\"/>"
                                        "<xacro:property name=\"mixed\" value=\"n${1+1}\"/>"
                                        "<xacro:property name=\"real\" value=\"${1/2}\"/>");
    REQUIRE(meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, silent).has_value());

    REQUIRE(scope.lookup("exact")->kind() == meios::value_kind::integer);
    REQUIRE(scope.lookup("exact")->integer() == std::optional<std::int64_t>(2));
    REQUIRE(scope.lookup("mixed")->kind() == meios::value_kind::string);
    REQUIRE(scope.lookup("mixed")->text() == std::optional<std::string>("n2"));
    REQUIRE(scope.lookup("real")->kind() == meios::value_kind::real);
}

TEST_CASE("a scalar keeps its kind across a property, a macro default, an argument and a nested property", "[native][value]")
{
    const meios::value scalars[] = {meios::value{}, meios::value{true}, integer_value(7), real_value(1.5), string_value("arm")};
    for(const meios::value &scalar : scalars)
    {
        INFO("kind " << static_cast<std::int32_t>(scalar.kind()));
        meios::eval_scope scope;
        scope.set("x", scalar);
        meios::source_stack sources;
        meios::log_sink silent;

        REQUIRE(meios::expand(carried_through_every_route(), scope, sources, "inline.xacro", meios::expansion_limits{}, silent).has_value());
        for(std::string_view name : {"copy", "from_default", "from_argument", "from_inner"})
        {
            INFO("binding " << name);
            const std::optional<meios::value> bound = scope.lookup(name);
            REQUIRE(bound.has_value());
            CHECK(bound->kind() == scalar.kind());
            CHECK(*bound == scalar);
        }
    }
}

TEST_CASE("a collection in mixed text fails typed, located and once, and names no content", "[native][value]")
{
    meios::eval_scope scope;
    scope.set("config", sample_mapping());
    meios::source_stack sources;
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    const std::string source = "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
                               "  <link name=\"base\"/>\n"
                               "  <l>name ${config}</l>\n"
                               "</robot>\n";

    const meios::expected<meios::expansion, meios::expansion_error> expanded = meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, sink);

    REQUIRE_FALSE(expanded.has_value());
    CHECK(expanded.error().code == meios::diagnostic_code::expression_error);
    CHECK(expanded.error().loc.file.filename() == "inline.xacro");
    CHECK(expanded.error().loc.line == 3);
    REQUIRE(counts.errors.size() == 1);
    CHECK(counts.errors.front().code == meios::diagnostic_code::expression_error);
    CHECK(counts.errors.front().loc.file.filename() == "inline.xacro");
    CHECK(counts.errors.front().loc.line == 3);
    CHECK(counts.errors.front().message.find("config") != std::string::npos);
    CHECK(counts.errors.front().message.find("mapping") != std::string::npos);
    CHECK(counts.errors.front().message.find("zulu") == std::string::npos);
}

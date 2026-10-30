#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstdint>
#include <optional>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

std::string robot(const std::string &body)
{
    return "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n" + body + "</robot>";
}

expansion_result expand_in(meios::eval_scope &scope, const std::string &body)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    return meios::expand(robot(body), scope, sources, "inline.xacro", meios::expansion_limits{}, silent);
}

expansion_result expand_body(const std::string &body)
{
    meios::eval_scope scope;
    return expand_in(scope, body);
}

bool renders_link(const expansion_result &out, const std::string &name)
{
    return out.has_value() && out->document.find("<link name=\"" + name + "\"/>") != std::string::npos;
}

}

TEST_CASE("a property read before the property it names is written renders", "[xacro][deferred]")
{
    const expansion_result out = expand_body("<xacro:property name=\"a\" value=\"${b}\"/>\n"
                                             "<xacro:property name=\"b\" value=\"1\"/>\n"
                                             "<link name=\"forward_${a}\"/>\n");
    REQUIRE(renders_link(out, "forward_1"));
}

TEST_CASE("a property is evaluated at its first read and keeps that value", "[xacro][deferred]")
{
    const expansion_result out = expand_body("<xacro:property name=\"b\" value=\"1\"/>\n"
                                             "<xacro:property name=\"p\" value=\"a${b}\"/>\n"
                                             "<xacro:property name=\"b\" value=\"2\"/>\n"
                                             "<link name=\"${p}\"/>\n"
                                             "<xacro:property name=\"b\" value=\"3\"/>\n"
                                             "<link name=\"x${p}\"/>\n");
    REQUIRE(renders_link(out, "a2"));
    REQUIRE(renders_link(out, "xa2"));
}

TEST_CASE("a property nothing reads is never evaluated", "[xacro][deferred]")
{
    REQUIRE(renders_link(expand_body("<xacro:property name=\"p\" value=\"${nope}\"/>\n<link name=\"l\"/>\n"), "l"));
    REQUIRE(renders_link(expand_body("<xacro:property name=\"p\" value=\"a$$x\"/>\n<link name=\"l\"/>\n"), "l"));
}

TEST_CASE("a default is deferred like a value and reads a property written after it", "[xacro][deferred]")
{
    const expansion_result out = expand_body("<xacro:property name=\"p\" default=\"${q}\"/>\n"
                                             "<xacro:property name=\"q\" value=\"Q\"/>\n"
                                             "<link name=\"a${p}\"/>\n");
    REQUIRE(renders_link(out, "aQ"));
}

TEST_CASE("a property read inside a macro body reads a global written after the macro", "[xacro][deferred]")
{
    const expansion_result out = expand_body("<xacro:property name=\"p\" value=\"a${g}\"/>\n"
                                             "<xacro:macro name=\"m\"><link name=\"${p}\"/></xacro:macro>\n"
                                             "<xacro:property name=\"g\" value=\"g\"/>\n"
                                             "<xacro:m/>\n");
    REQUIRE(renders_link(out, "ag"));
}

TEST_CASE("a circular definition refuses and names every step of the cycle", "[xacro][deferred]")
{
    const expansion_result out = expand_body("<xacro:property name=\"a\" value=\"${b}\"/>\n"
                                             "<xacro:property name=\"b\" value=\"${a}\"/>\n"
                                             "<link name=\"l${a}\"/>\n");
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::xacro_structural_error);
    CHECK(out.error().message
          == "circular variable definition: a (inline.xacro:2) -> b (inline.xacro:3) -> a");
    CHECK(out.error().loc.line == 3);
}

TEST_CASE("a property whose value reads its own name refuses as a cycle", "[xacro][deferred]")
{
    const expansion_result out = expand_body("<xacro:property name=\"a\" value=\"1\"/>\n"
                                             "<xacro:property name=\"a\" value=\"${a+1}\"/>\n"
                                             "<link name=\"l${a}\"/>\n");
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::xacro_structural_error);
    CHECK(out.error().message == "circular variable definition: a (inline.xacro:3) -> a");
}

TEST_CASE("after expand an unread deferred property reads absent and every other one reads its value",
          "[xacro][deferred]")
{
    meios::eval_scope scope;
    REQUIRE(expand_in(scope, "<xacro:property name=\"unread\" value=\"${1+1}\"/>\n"
                             "<xacro:property name=\"read\" value=\"${2+2}\"/>\n"
                             "<link name=\"l${read}\"/>\n"
                             "<xacro:property name=\"eager\" value=\"${3+3}\" lazy_eval=\"false\"/>\n"
                             "<xacro:property name=\"global\" value=\"${4+4}\" scope=\"global\"/>\n")
                .has_value());
    CHECK_FALSE(scope.lookup("unread").has_value());
    CHECK(scope.contains("unread"));
    CHECK(scope.lookup("read") == std::optional<meios::value>(meios::value{ std::int64_t{ 4 } }));
    CHECK(scope.lookup("eager") == std::optional<meios::value>(meios::value{ std::int64_t{ 6 } }));
    CHECK(scope.lookup("global") == std::optional<meios::value>(meios::value{ std::int64_t{ 8 } }));
}

TEST_CASE("a second expansion over one scope reads what the first left unread as absent", "[xacro][deferred]")
{
    meios::eval_scope scope;
    REQUIRE(expand_in(scope, "<xacro:property name=\"p\" value=\"a${1}\"/>\n").has_value());
    const expansion_result out = expand_in(scope, "<link name=\"${p}\"/>\n");
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::undefined_property);
}

TEST_CASE("an empty value and an empty default bind the empty string", "[xacro][deferred]")
{
    meios::eval_scope scope;
    REQUIRE(renders_link(expand_in(scope, "<xacro:property name=\"v\" value=\"\"/>\n"
                                          "<xacro:property name=\"d\" default=\"\"/>\n"
                                          "<link name=\"l${v}${d}\"/>\n"),
                         "l"));
    CHECK(scope.lookup("v") == std::optional<meios::value>(meios::value{ std::string() }));
    CHECK(scope.lookup("d") == std::optional<meios::value>(meios::value{ std::string() }));
}

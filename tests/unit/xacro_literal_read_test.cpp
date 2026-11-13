#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <utility>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_body(const std::string &body)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string source =
        R"(<robot name="r" xmlns:xacro="http://www.ros.org/wiki/xacro">)" + body + "</robot>";
    return meios::expand(source, scope, sources, "literal.xacro", meios::expansion_limits{}, silent);
}

bool renders(const std::string &body, const std::string &name)
{
    const expansion_result out = expand_body(body);
    REQUIRE(out.has_value());
    return out->document.find("name=\"" + name + "\"") != std::string::npos;
}

std::string quoted_properties(const std::string &lazy)
{
    return R"(<xacro:property name="q1" value="'abc'")" + lazy + "/>"
         + R"(<xacro:property name="q2" value="''")" + lazy + "/>"
         + R"(<xacro:property name="q3" value="'a'b'")" + lazy + "/>"
         + R"(<link name="x${q1}"/><link name="y${q2}"/><link name="z${q3}"/>)";
}

// Each authored value against what the pinned reference renders for it, read as [${p}].
const std::array<std::pair<const char *, const char *>, 17> measured{ {
        {"1_0", "1_0"}, {" 1", "1"}, {"+1", "1"}, {"\xd9\xa1", "1"}, {"'abc'", "abc"}, {"''", ""},
        {"'a'b'", "a'b"}, {"1e3", "1000.0"}, {"1.50", "1.5"}, {"-0", "0"}, {"00012", "12"},
        {"1.0e+20", "1e+20"}, {" 1.5e1 ", "15.0"}, {"0x10", "0x10"}, {"TRUE", "TRUE"},
        {" true ", " true "}, {"99999999999999999999", "99999999999999999999"},
} };

std::string bracketed(const std::string &authored, const std::string &lazy)
{
    return R"(<xacro:property name="p" value=")" + authored + "\"" + lazy + R"(/><link name="[${p}]"/>)";
}

const std::string wide = "99999999999999999999";

std::string wide_bound(const std::string &digits)
{
    return R"(<xacro:property name="b" value=")" + digits + "\"/>";
}

}

TEST_CASE("a lazy property written in single quotes binds the text between them", "[xacro][literal]")
{
    const std::string body = quoted_properties("");
    CHECK(renders(body, "xabc"));
    CHECK(renders(body, "y"));
    CHECK(renders(body, "za'b"));
}

TEST_CASE("an eager property written in single quotes binds the text between them", "[xacro][literal]")
{
    const std::string body = quoted_properties(R"( lazy_eval="false")");
    CHECK(renders(body, "xabc"));
    CHECK(renders(body, "y"));
    CHECK(renders(body, "za'b"));
}

TEST_CASE("a quoted lazy property defers the expression between its quotes", "[xacro][literal]")
{
    CHECK(renders(R"(<xacro:property name="q" value="'${x}'"/><xacro:property name="x" value="v"/>)"
                  R"(<link name="[${q}]"/>)",
                  "[v]"));
}

TEST_CASE("each measured spelling of a property renders as the reference renders it", "[xacro][literal]")
{
    for(const auto &[authored, rendered] : measured)
    {
        CAPTURE(authored);
        CHECK(renders(bracketed(authored, ""), std::string("[") + rendered + "]"));
        CHECK(renders(bracketed(authored, R"( lazy_eval="false")"), std::string("[") + rendered + "]"));
    }
}

TEST_CASE("a lazy quoted value with no expression is read again, as resolving it would", "[xacro][literal]")
{
    CHECK(renders(R"(<xacro:property name="q" value="'1'"/><link name="${q + 1}"/>)", "2"));
    CHECK(renders(R"(<xacro:property name="q" value="''abc''"/><link name="[${q}]"/>)", "[abc]"));
    CHECK(renders(R"(<xacro:property name="q" value="''abc''" lazy_eval="false"/><link name="[${q}]"/>)",
                  "['abc']"));
    CHECK_FALSE(expand_body(R"(<xacro:property name="q" value="'1'" lazy_eval="false"/><link name="${q + 1}"/>)")
                        .has_value());
}

TEST_CASE("a macro argument is read as a literal", "[xacro][literal]")
{
    CHECK(renders(R"(<xacro:macro name="m" params="a b c"><link name="${a}_${b}_${c}"/></xacro:macro>)"
                  R"(<xacro:m a="'q'" b=" 1" c="+2"/>)",
                  "q_1_2"));
}

TEST_CASE("a macro default is read as a literal, an inherited one included", "[xacro][literal]")
{
    CHECK(renders(R"(<xacro:macro name="m" params="a:='q'"><link name="[${a}]"/></xacro:macro><xacro:m/>)",
                  "[q]"));
    CHECK(renders(R"(<xacro:property name="a" value="''q''" lazy_eval="false"/>)"
                  R"(<xacro:macro name="m" params="a:=^"><link name="[${a}]"/></xacro:macro><xacro:m/>)",
                  "[q]"));
}

TEST_CASE("a deferred property is read as a literal once resolved", "[xacro][literal]")
{
    CHECK(renders(R"(<xacro:property name="d" value="${'+7'}"/><link name="[${d + 1}]"/>)", "[8]"));
    CHECK(renders(R"(<xacro:property name="d" value="''${x}''"/><xacro:property name="x" value="v"/>)"
                  R"(<link name="[${d}]"/>)",
                  "[v]"));
}

TEST_CASE("an integer beyond 64 bits keeps its digits and refuses arithmetic", "[xacro][literal]")
{
    const std::string big = R"(<xacro:property name="b" value="99999999999999999999"/>)";
    CHECK(renders(big + R"(<link name="[${b}]"/>)", "[99999999999999999999]"));
    CHECK_FALSE(expand_body(big + R"(<link name="${b + 1}"/>)").has_value());
}

TEST_CASE("a lazy property forwarding a wide integer renders its digits", "[xacro][literal]")
{
    for(const std::string &digits : { wide, "-" + wide })
    {
        CAPTURE(digits);
        CHECK(renders(wide_bound(digits) + R"(<xacro:property name="c" value="${b}"/><link name="c_${c}"/>)",
                      "c_" + digits));
    }
}

TEST_CASE("an eager property and a macro argument forwarding a wide integer render its digits",
          "[xacro][literal]")
{
    const std::string bound = wide_bound(wide);
    CHECK(renders(bound + R"(<xacro:property name="c" value="${b}" lazy_eval="false"/><link name="c_${c}"/>)",
                  "c_" + wide));
    CHECK(renders(bound + R"(<xacro:macro name="m" params="a"><link name="m_${a}"/></xacro:macro>)"
                  R"(<xacro:m a="${b}"/>)",
                  "m_" + wide));
    CHECK(renders(bound + R"(<xacro:macro name="m" params="a:=${b}"><link name="m_${a}"/></xacro:macro><xacro:m/>)",
                  "m_" + wide));
}

TEST_CASE("arithmetic on a forwarded wide integer refuses", "[xacro][literal]")
{
    const std::string forwarded = wide_bound(wide) + R"(<xacro:property name="c" value="${b}"/>)";
    CHECK_FALSE(expand_body(forwarded + R"(<link name="${c + 1}"/>)").has_value());
    CHECK_FALSE(expand_body(forwarded + R"(<link name="${c * 2}"/>)").has_value());
}

TEST_CASE("a real beyond the double range and an infinity spelling keep their text", "[xacro][literal]")
{
    CHECK(renders(bracketed("INFINITY", ""), "[INFINITY]"));
    CHECK(renders(bracketed("1e400", ""), "[1e400]"));
    CHECK(renders(bracketed("1e400", R"( lazy_eval="false")"), "[1e400]"));
}

TEST_CASE("an argument is bound as written, not read as a literal", "[xacro][literal]")
{
    CHECK(renders(R"x(<xacro:arg name="a" default="'x'"/><link name="$(arg a)"/>)x", "'x'"));
    CHECK(renders(R"x(<xacro:arg name="a" default=" 1"/><link name="[$(arg a)]"/>)x", "[ 1]"));
}

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <cstddef>
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
    return meios::expand(source, scope, sources, "wide.xacro", meios::expansion_limits{}, silent);
}

bool renders(const std::string &body, const std::string &name)
{
    const expansion_result out = expand_body(body);
    REQUIRE(out.has_value());
    return out->document.find("name=\"" + name + "\"") != std::string::npos;
}

const std::string wide = "99999999999999999999";

std::string lazily_forwarded(const std::string &digits)
{
    return R"(<xacro:property name="b" value=")" + digits + R"("/><xacro:property name="c" value="${b}"/>)";
}

// Each way a value reaches c through one exact span: a lazy property, an eager one, a macro
// argument and a macro default, the body placed where c is bound.
const std::array<std::pair<const char *, const char *>, 4> forwarders{ {
        {R"(<xacro:property name="c" value="${b}"/>)", ""},
        {R"(<xacro:property name="c" value="${b}" lazy_eval="false"/>)", ""},
        {R"(<xacro:macro name="m" params="c">)", R"(</xacro:macro><xacro:m c="${b}"/>)"},
        {R"(<xacro:macro name="m" params="c:=${b}">)", R"(</xacro:macro><xacro:m/>)"},
} };

std::string forwarded(std::size_t way, const std::string &body)
{
    return R"(<xacro:property name="b" value=")" + wide + R"("/>)" + forwarders[way].first + body + forwarders[way].second;
}

}

TEST_CASE("a forwarded wide integer takes part in an ordering comparison", "[xacro][wide]")
{
    CHECK(renders(lazily_forwarded(wide) + R"(<link name="l_${c &gt; 0}"/>)", "l_True"));
    CHECK(renders(lazily_forwarded("-" + wide) + R"(<link name="l_${c &lt; 0}"/>)", "l_True"));
    CHECK(renders(lazily_forwarded(wide) + R"(<link name="l_${c &gt; 9223372036854775807}"/>)", "l_True"));
}

TEST_CASE("a wide integer forwarded every way compares exactly with the integer it came from and with reals", "[xacro][wide]")
{
    for(std::size_t way = 0; way < forwarders.size(); ++way)
    {
        INFO(forwarders[way].first);
        CHECK(renders(forwarded(way, R"(<link name="l_${c == b}"/>)"), "l_True"));
        CHECK(renders(forwarded(way, R"(<link name="l_${c &lt; 1e20}_${c == 1e20}_${0 &lt; c &lt; 1e21}"/>)"), "l_True_False_True"));
    }
}

TEST_CASE("a conditional tests a comparison of a forwarded wide integer", "[xacro][wide]")
{
    for(std::size_t way = 0; way < forwarders.size(); ++way)
    {
        INFO(forwarders[way].first);
        CHECK(renders(forwarded(way, R"(<xacro:if value="${c &gt; 1}"><link name="taken"/></xacro:if>)"), "taken"));
        CHECK(renders(forwarded(way, R"(<xacro:unless value="${c &lt; 0}"><link name="taken"/></xacro:unless>)"), "taken"));
    }
}

TEST_CASE("a comparison of a forwarded wide integer reads as the lazy attribute of a property", "[xacro][wide]")
{
    const std::string lazy = R"(<xacro:property name="p" value="${q}" lazy_eval="${c &gt; 0}"/><xacro:property name="q" value="5"/>)";
    CHECK(renders(lazily_forwarded(wide) + lazy + R"(<link name="l_${p}"/>)", "l_5"));
    const std::string eager = R"(<xacro:property name="p" value="${q}" lazy_eval="${c &lt; 0}"/><xacro:property name="q" value="5"/>)";
    CHECK_FALSE(expand_body(lazily_forwarded(wide) + eager + R"(<link name="l_${p}"/>)").has_value());
}

TEST_CASE("arithmetic on a forwarded wide integer still refuses under the built-in evaluator", "[xacro][wide]")
{
    CHECK_FALSE(expand_body(lazily_forwarded(wide) + R"(<link name="l_${c + 1}"/>)").has_value());
}

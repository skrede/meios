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

TEST_CASE("a forwarded wide integer adds and subtracts exactly, back to a 64-bit integer where it fits", "[xacro][wide]")
{
    for(std::size_t way = 0; way < forwarders.size(); ++way)
    {
        INFO(forwarders[way].first);
        CHECK(renders(forwarded(way, R"(<link name="l_${c + 1}"/>)"), "l_100000000000000000000"));
        CHECK(renders(forwarded(way, R"(<link name="l_${c - b}"/>)"), "l_0"));
    }
}

TEST_CASE("a wide integer read by name or forwarded negates, multiplies and raises exactly", "[xacro][wide]")
{
    const std::pair<const char *, const char *> measured[] = {
            {"${-c}", "-99999999999999999999"}, {"${+c}", "99999999999999999999"}, {"${--c}", "99999999999999999999"},
            {"${c * 2}", "199999999999999999998"}, {"${c * c}", "9999999999999999999800000000000000000001"},
            {"${c ** 2}", "9999999999999999999800000000000000000001"}, {"${b + 1}", "100000000000000000000"},
            {"${c - 1}", "99999999999999999998"}, {"${2 ** 70}", "1180591620717411303424"},
    };
    for(const auto &[expression, digits] : measured)
    {
        INFO(expression);
        CHECK(renders(lazily_forwarded(wide) + R"(<link name="l_)" + expression + R"("/>)", std::string("l_") + digits));
    }
}

TEST_CASE("a forwarded wide integer divides by floor division toward negative infinity", "[xacro][wide]")
{
    const std::string link = R"(<link name="l_${c // 7}_${-c // 7}_${c // -7}"/>)";
    CHECK(renders(lazily_forwarded(wide) + link, "l_14285714285714285714_-14285714285714285715_-14285714285714285715"));
}

TEST_CASE("a forwarded wide integer takes a remainder, divides and reads as the nearest real beside a real", "[xacro][wide]")
{
    const std::pair<const char *, const char *> measured[] = {
            {"${c % 7}_${-c % 7}_${c % -7}", "1_6_-6"}, {"${c // c}", "1"}, {"${c / c}", "1.0"}, {"${floor(c)}", "99999999999999999999"},
            {"${c / 3}_${-c / 7}_${7 / c}", "3.333333333333333e+19_-1.4285714285714287e+19_7e-20"}, {"${c * 0.5}", "5e+19"},
            {"${c + 1.5}_${c // 2.5}_${c % 2.5}", "1e+20_4e+19_0.0"}, {"${c ** 0.5}_${c ** -1}", "10000000000.0_1e-20"},
    };
    for(const auto &[expression, render] : measured)
    {
        INFO(expression);
        CHECK(renders(lazily_forwarded(wide) + R"(<link name="l_)" + expression + R"("/>)", std::string("l_") + render));
    }
}

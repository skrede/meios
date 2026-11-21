#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstdint>

namespace
{

using meios::diagnostic_code;
using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_body(const std::string &body)
{
    meios::log_sink log;
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string document = "<robot name=\"probe\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n" + body + "\n</robot>";
    return meios::expand(document, scope, sources, "condition.xacro", meios::expansion_limits{}, log);
}

std::string conditional(const char *tag, const std::string &attributes)
{
    return std::string("<xacro:") + tag + attributes + "><link name=\"t\"/></xacro:" + tag + ">";
}

bool takes_branch(const char *tag, const std::string &attributes)
{
    const expansion_result out = expand_body(conditional(tag, attributes));
    REQUIRE(out.has_value());
    return out->document.find("<link name=\"t\"") != std::string::npos;
}

void require_structural_refusal(const expansion_result &out, std::int32_t line)
{
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == diagnostic_code::xacro_structural_error);
    CHECK(out.error().loc.line == line);
}

}

TEST_CASE("a condition written in a non-ASCII decimal digit takes its branch", "[xacro][condition]")
{
    REQUIRE(takes_branch("if", " value=\"١\""));
}

TEST_CASE("integer text with an underscore or surrounding space takes its branch", "[xacro][condition]")
{
    REQUIRE(takes_branch("if", " value=\"1_0\""));
    REQUIRE(takes_branch("if", " value=\" 1\""));
}

TEST_CASE("a boolean spelled in another letter case refuses at the conditional", "[xacro][condition]")
{
    require_structural_refusal(expand_body(conditional("if", " value=\"TRUE\"")), 2);
}

TEST_CASE("an empty or missing condition refuses at the conditional", "[xacro][condition]")
{
    require_structural_refusal(expand_body(conditional("if", " value=\"\"")), 2);
    require_structural_refusal(expand_body(conditional("if", "")), 2);
    require_structural_refusal(expand_body(conditional("unless", "")), 2);
}

TEST_CASE("text an expression produced is not evaluated a second time", "[xacro][condition]")
{
    require_structural_refusal(expand_body(conditional("if", " value=\"${'a'}\"")), 2);
}

TEST_CASE("a zero real takes an unless branch and an empty produced text refuses", "[xacro][condition]")
{
    REQUIRE(takes_branch("unless", " value=\"0.0\""));
    REQUIRE_FALSE(takes_branch("if", " value=\"${0.0}\""));
    require_structural_refusal(expand_body(conditional("if", " value=\"${''}\"")), 2);
}

TEST_CASE("a condition padding one expression span with spaces is read by what the span yields", "[xacro][condition]")
{
    REQUIRE(takes_branch("if", " value=\" ${True} \""));
    REQUIRE_FALSE(takes_branch("if", " value=\" ${0} \""));
}

TEST_CASE("a property's remove reads by the same rule, and an empty one as its default", "[xacro][condition]")
{
    require_structural_refusal(expand_body("<xacro:property name=\"p\" value=\"1\" remove=\"TRUE\"/>"), 2);
    const expansion_result kept = expand_body("<xacro:property name=\"p\" value=\"1\" remove=\"\"/><link name=\"l${p}\"/>");
    REQUIRE(kept.has_value());
    CHECK(kept->document.find("<link name=\"l1\"") != std::string::npos);
}

TEST_CASE("an empty lazy_eval reads as lazy while an empty condition refuses", "[xacro][condition]")
{
    const expansion_result lazy = expand_body("<xacro:property name=\"x\" value=\"1\"/><xacro:property name=\"b\" value=\"${x}\" lazy_eval=\"\"/>"
                                              "<xacro:property name=\"x\" value=\"2\"/><link name=\"l${b}\"/>");
    REQUIRE(lazy.has_value());
    CHECK(lazy->document.find("<link name=\"l2\"") != std::string::npos);
    require_structural_refusal(expand_body(conditional("if", " value=\"\"")), 2);
}

TEST_CASE("text a logical operator yields refuses as a condition unless it spells a boolean", "[xacro][condition]")
{
    require_structural_refusal(expand_body(conditional("if", " value=\"${'' or 'x'}\"")), 2);
    require_structural_refusal(expand_body(conditional("unless", " value=\"${'' and 'x'}\"")), 2);
    REQUIRE(takes_branch("if", " value=\"${'' or 'true'}\""));
}

TEST_CASE("a number a logical operator yields is read by its truth", "[xacro][condition]")
{
    REQUIRE(takes_branch("if", " value=\"${0 or 2}\""));
    REQUIRE_FALSE(takes_branch("if", " value=\"${2 and 0}\""));
}

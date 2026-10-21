#include "xacro_read_probe.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

const std::string head = "<robot name=\"p\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
                         "<xacro:property name=\"x\" value=\"vx\"/><xacro:arg name=\"y\" default=\"Y\"/>\n";

expansion_result expand_doc(const std::string &body, meios::eval_policy policy, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(head + body + "</robot>", scope, sources, "inline.xacro",
                         meios::expansion_limits{}, policy, {}, log);
}

expansion_result expand_doc(const std::string &body, meios::eval_policy policy = meios::eval_policy::fail)
{
    meios::log_sink_f log{ [](auto &&...) {} };
    return expand_doc(body, policy, log);
}

expansion_result expand_name(const std::string &name, meios::eval_policy policy = meios::eval_policy::fail)
{
    return expand_doc("<link name=\"" + name + "\"/>", policy);
}

void require_refused(const expansion_result &out, meios::diagnostic_code code, const std::string &message)
{
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == code);
    CHECK(out.error().message == message);
}

}

TEST_CASE("a stray double dollar straight after a span refuses before the span is evaluated",
          "[xacro][lex_lookahead]")
{
    const std::pair<const char *, const char *> table[] = {
        { "${nope}$$x", "invalid expression: $$x" },  { "$(arg unset)$$x", "invalid expression: $$x" },
        { "${1/0}$$x", "invalid expression: $$x" },   { "${'$$'}$$x", "invalid expression: $$x" },
        { "${x}$$x", "invalid expression: $$x" },     { "${nope}$$", "invalid expression: $$" },
    };
    for(const auto &[raw, message] : table)
    {
        INFO("raw: " << raw);
        require_refused(expand_name(raw), meios::diagnostic_code::expression_error, message);
    }
}

TEST_CASE("an unterminated span straight after a span refuses before the span is evaluated",
          "[xacro][lex_lookahead]")
{
    const std::pair<const char *, const char *> table[] = {
        { "${nope}${x", "unterminated ${ substitution span at offset 7" },
        { "${nope}$(x", "unterminated $( substitution span at offset 7" },
        { "$(arg unset)${x", "unterminated ${ substitution span at offset 12" },
    };
    for(const auto &[raw, message] : table)
    {
        INFO("raw: " << raw);
        require_refused(expand_name(raw), meios::diagnostic_code::unterminated_substitution, message);
    }
}

TEST_CASE("a token that lexes after a failing span leaves the span's own cause", "[xacro][lex_lookahead]")
{
    for(const char *raw : { "${nope}a$$x", "${nope}$x$$y", "${nope}$${x}$$y", "${nope}$$$(x", "${nope}$", "${nope}${x}$$y",
                            "${nope}${'a}'", "${nope}${a{b}" })
    {
        INFO("raw: " << raw);
        require_refused(expand_name(raw), meios::diagnostic_code::undefined_property, "name 'nope' is not defined");
    }
}

// The link sits on line 3; its value begins at 1-based column 13, so the run after the span is at 20.
TEST_CASE("the refusal is located at the token after the span", "[xacro][lex_lookahead]")
{
    for(const char *raw : { "${nope}$$x", "${nope}${x" })
    {
        INFO("raw: " << raw);
        const expansion_result out = expand_name(raw);
        REQUIRE_FALSE(out.has_value());
        CHECK(out.error().loc.line == 3);
        CHECK(out.error().loc.column == 20);
    }
}

TEST_CASE("the lookahead holds in a text node and in a condition", "[xacro][lex_lookahead]")
{
    require_refused(expand_doc("<link name=\"l\"><t>${nope}$$x</t></link>"), meios::diagnostic_code::expression_error,
                    "invalid expression: $$x");
    require_refused(expand_doc("<xacro:if value=\"${nope}$$x\"><link name=\"l\"/></xacro:if>"),
                    meios::diagnostic_code::expression_error, "invalid expression: $$x");
}

TEST_CASE("a lookahead refusal is terminal under a lenient policy", "[xacro][lex_lookahead]")
{
    for(meios::eval_policy policy : { meios::eval_policy::warn, meios::eval_policy::skip })
    {
        require_refused(expand_name("${[1, 2]}$$x", policy), meios::diagnostic_code::expression_error,
                        "invalid expression: $$x");
        const expansion_result unterminated = expand_name("${[1, 2]}${x", policy);
        REQUIRE_FALSE(unterminated.has_value());
        CHECK(unterminated.error().code == meios::diagnostic_code::unterminated_substitution);
    }
}

TEST_CASE("an expression the core leaves verbatim under a lenient policy is still kept", "[xacro][lex_lookahead]")
{
    const expansion_result out = expand_name("${[1, 2]}$x", meios::eval_policy::warn);
    REQUIRE(out.has_value());
    CHECK(out->document.find("name=\"${[1, 2]}$x\"") != std::string::npos);
}

TEST_CASE("an unterminated span inside a command is not reported as kept verbatim under a lenient policy",
          "[xacro][lex_lookahead]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    const expansion_result out = expand_doc("<link name=\"${[1, 2]}$(arg ${x)\"/>", meios::eval_policy::warn, log);
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::unterminated_substitution);
    CHECK(xacro_probe::coded(records, meios::diagnostic_code::unterminated_substitution) == 1);
    CHECK(xacro_probe::coded(records, meios::diagnostic_code::unsupported_expression) == 1);
    for(const xacro_probe::captured &r : records)
        if(r.code == meios::diagnostic_code::unterminated_substitution)
            CHECK(r.lvl == meios::level::error);
}

TEST_CASE("a nested command after a span the core leaves verbatim refuses under a lenient policy",
          "[xacro][lex_lookahead]")
{
    for(meios::eval_policy policy : { meios::eval_policy::warn, meios::eval_policy::skip })
        for(const char *raw : { "${[1, 2]}$(arg $(x))", "${[1, 2]}${'$(arg $(x))'}" })
        {
            INFO("raw: " << raw << ", warn: " << (policy == meios::eval_policy::warn));
            std::vector<xacro_probe::captured> records;
            meios::log_sink_f log{ xacro_probe::recorder{ records } };
            const expansion_result out = expand_doc("<link name=\"" + std::string(raw) + "\"/>", policy, log);
            REQUIRE_FALSE(out.has_value());
            CHECK(out.error().code == meios::diagnostic_code::unterminated_substitution);
            CHECK(xacro_probe::coded(records, meios::diagnostic_code::unsupported_expression)
                  == (policy == meios::eval_policy::warn ? 1u : 0u));
            CHECK(xacro_probe::coded(records, meios::diagnostic_code::unterminated_substitution) == 1);
            for(const xacro_probe::captured &r : records)
                if(r.code == meios::diagnostic_code::unterminated_substitution)
                    CHECK(r.lvl == meios::level::error);
        }
}

// Each nested span charges three expression-depth units, so 86 of them cross the default ceiling of 256.
TEST_CASE("a span past the expression-depth ceiling after a span the core leaves verbatim refuses under a lenient policy",
          "[xacro][lex_lookahead]")
{
    std::string nested = "${[1, 2]}";
    for(int i = 0; i < 86; ++i)
        nested += "${";
    nested += "1" + std::string(86, '}');
    for(meios::eval_policy policy : { meios::eval_policy::warn, meios::eval_policy::skip })
    {
        INFO("warn: " << (policy == meios::eval_policy::warn));
        std::vector<xacro_probe::captured> records;
        meios::log_sink_f log{ xacro_probe::recorder{ records } };
        const expansion_result out = expand_doc("<link name=\"" + nested + "\"/>", policy, log);
        REQUIRE_FALSE(out.has_value());
        CHECK(out.error().code == meios::diagnostic_code::expansion_budget_exceeded);
        CHECK(xacro_probe::coded(records, meios::diagnostic_code::unsupported_expression)
              == (policy == meios::eval_policy::warn ? 1u : 0u));
        CHECK(xacro_probe::errors(records) == 1);
    }
}

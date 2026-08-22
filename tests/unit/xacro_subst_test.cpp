#include "xacro_subst_fixture.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
void portable_setenv(const char *name, const char *value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

struct refusing_row
{
    const char *raw;
    const char *subject;
};

// Every refusal the two substitution stems drive, in one table: the two package-locating
// forms, the two argument forms, the two environment forms, the optional-environment
// form, the unknown command, the recursive default, the double-quoted string spelling
// and the scanner's own unterminated spans.
const refusing_row refusing_rows[] = {
    { "$(find)", "a find with no package name" },
    { "$(find missing)", "an unresolvable package" },
    { "$(arg)", "an arg with no name" },
    { "$(arg height)", "an unset arg with no default" },
    { "$(arg height $(find absent))", "a default whose inner locate fails" },
    { "$(env)", "an env with no variable name" },
    { "$(env MEIOS_SUBST_SWEEP_ABSENT)", "an absent environment variable" },
    { "$(optenv)", "an optenv with no variable name" },
    { "$(bogus x)", "an unknown command" },
    { "${\"a{b\"}", "the unmeasured double-quoted string spelling" },
    { "${prefix", "an unterminated expression span" },
    { "$(find pkg", "an unterminated command span" },
};

}

TEST_CASE("no substitution refusal reaches the caller carrying the unspecified code",
          "[xacro][subst][scan]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;
    std::filesystem::path document{ "robot.xacro" };

    for(const refusing_row &row : refusing_rows)
    {
        const substitution_result out = meios::substitute(row.raw, scope, sources, document, log);
        CHECK_FALSE(out.has_value());
        if(out.has_value())
            continue;
        INFO(row.subject << " => " << meios::to_string(out.error().code));
        CHECK(out.error().code != meios::diagnostic_code::unspecified);
        CHECK(locates_a_file(out));
    }
}

TEST_CASE("substitution scanner concatenates literal, expression and command spans",
          "[xacro][subst][scan]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    std::filesystem::path document{ "robot.xacro" };

    meios::eval_scope scope;
    scope.set("prefix", meios::value{ std::string("arm") });
    scope.set("suffix", meios::value{ std::string("1") });
    scope.set("radius", *meios::value::make_real(0.2));

    SECTION("a string property span concatenates with a trailing literal")
    {
        const substitution_result out =
            meios::substitute("${prefix}_link", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "arm_link");
    }

    SECTION("a numeric expression span stringifies the evaluated value")
    {
        const substitution_result out =
            meios::substitute("${radius*2}", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "0.4");
    }

    SECTION("adjacent spans concatenate left to right")
    {
        const substitution_result out =
            meios::substitute("${prefix}_${suffix}", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "arm_1");
    }

    SECTION("a bare dollar without a span is literal")
    {
        const substitution_result out =
            meios::substitute("price $5 only", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "price $5 only");
    }

    SECTION("an opening brace inside a string literal is that literal's text")
    {
        const substitution_result out =
            meios::substitute("${'a{b'}", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "a{b");
    }

    // Upstream refuses this spelling as an unterminated string literal; meios renders it,
    // a deliberate divergence rather than an accident of the scan.
    SECTION("a closing brace inside a string literal does not end the span")
    {
        const substitution_result out =
            meios::substitute("${'a}b'}", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "a}b");
    }

    SECTION("an unterminated expression span loud-fails")
    {
        const substitution_result out =
            meios::substitute("${prefix", scope, sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::unterminated_substitution);
        REQUIRE(locates_a_file(out));
        REQUIRE(any_contains(records, "unterminated"));
    }

    SECTION("an unterminated command span loud-fails")
    {
        const substitution_result out =
            meios::substitute("$(find pkg", scope, sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::unterminated_substitution);
        REQUIRE(locates_a_file(out));
        REQUIRE(any_contains(records, "unterminated"));
    }
}

TEST_CASE("substitution logs every environment read without echoing values",
          "[xacro][subst][env]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;
    std::filesystem::path document{ "robot.xacro" };

    portable_setenv("MEIOS_SUBST_ENV_PROBE", "confidential-token-xyz");

    SECTION("$(env) substitutes the value and logs the name only")
    {
        const substitution_result out =
            meios::substitute("$(env MEIOS_SUBST_ENV_PROBE)", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "confidential-token-xyz");
        REQUIRE(records_at(records, meios::level::info) == 1);
        REQUIRE(any_contains(records, "MEIOS_SUBST_ENV_PROBE"));
        REQUIRE_FALSE(any_contains(records, "confidential-token-xyz"));
    }

    SECTION("a missing $(env) loud-fails but still logs the read")
    {
        const substitution_result out =
            meios::substitute("$(env MEIOS_SUBST_ENV_ABSENT)", scope, sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::unresolved_env);
        REQUIRE(locates_a_file(out));
        REQUIRE(records_at(records, meios::level::info) == 1);
        REQUIRE(any_contains(records, "MEIOS_SUBST_ENV_ABSENT"));
        REQUIRE(any_contains(records, "is not set"));
    }

    SECTION("$(optenv) falls back to its default and still logs the read")
    {
        const substitution_result out = meios::substitute(
            "$(optenv MEIOS_SUBST_ENV_ABSENT fallback)", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "fallback");
        REQUIRE(records_at(records, meios::level::info) == 1);
        REQUIRE(any_contains(records, "MEIOS_SUBST_ENV_ABSENT"));
    }
}

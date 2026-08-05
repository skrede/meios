#include "xacro_subst_fixture.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
int info_notes(const std::vector<std::pair<meios::level, std::string>> &records)
{
    int count = 0;
    for(const std::pair<meios::level, std::string> &entry : records)
        if(entry.first == meios::level::info)
            ++count;
    return count;
}
void portable_setenv(const char *name, const char *value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

}

TEST_CASE("substitution scanner concatenates literal, expression and command spans",
          "[xacro][subst][scan]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    std::filesystem::path document;

    meios::eval_scope scope;
    scope.set("prefix", meios::binding{ std::string("arm") });
    scope.set("suffix", meios::binding{ std::string("1") });
    scope.set("radius", meios::binding{ meios::value{ 0.2 } });

    SECTION("a string property span concatenates with a trailing literal")
    {
        meios::substitution out = meios::substitute("${prefix}_link", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == "arm_link");
    }

    SECTION("a numeric expression span stringifies the evaluated value")
    {
        meios::substitution out = meios::substitute("${radius*2}", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == "0.4");
    }

    SECTION("adjacent spans concatenate left to right")
    {
        meios::substitution out = meios::substitute("${prefix}_${suffix}", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == "arm_1");
    }

    SECTION("a bare dollar without a span is literal")
    {
        meios::substitution out = meios::substitute("price $5 only", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == "price $5 only");
    }

    SECTION("an unterminated expression span loud-fails")
    {
        meios::substitution out = meios::substitute("${prefix", scope, sources, document, log);
        REQUIRE_FALSE(out.ok);
        REQUIRE(any_contains(records, "unterminated"));
    }

    SECTION("an unterminated command span loud-fails")
    {
        meios::substitution out = meios::substitute("$(find pkg", scope, sources, document, log);
        REQUIRE_FALSE(out.ok);
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
    std::filesystem::path document;

    portable_setenv("MEIOS_SUBST_ENV_PROBE", "confidential-token-xyz");

    SECTION("$(env) substitutes the value and logs the name only")
    {
        meios::substitution out =
            meios::substitute("$(env MEIOS_SUBST_ENV_PROBE)", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == "confidential-token-xyz");
        REQUIRE(info_notes(records) == 1);
        REQUIRE(any_contains(records, "MEIOS_SUBST_ENV_PROBE"));
        REQUIRE_FALSE(any_contains(records, "confidential-token-xyz"));
    }

    SECTION("a missing $(env) loud-fails but still logs the read")
    {
        meios::substitution out =
            meios::substitute("$(env MEIOS_SUBST_ENV_ABSENT)", scope, sources, document, log);
        REQUIRE_FALSE(out.ok);
        REQUIRE(info_notes(records) == 1);
        REQUIRE(any_contains(records, "MEIOS_SUBST_ENV_ABSENT"));
        REQUIRE(any_contains(records, "is not set"));
    }

    SECTION("$(optenv) falls back to its default and still logs the read")
    {
        meios::substitution out =
            meios::substitute("$(optenv MEIOS_SUBST_ENV_ABSENT fallback)", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == "fallback");
        REQUIRE(info_notes(records) == 1);
        REQUIRE(any_contains(records, "MEIOS_SUBST_ENV_ABSENT"));
    }
}

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstdlib>
#include <utility>
#include <filesystem>
#include <string_view>

namespace
{

struct captured_log
{
    std::vector<std::pair<meios::level, std::string>> &records;

    void operator()(meios::level lvl, const std::string &message)
    {
        records.push_back({ lvl, message });
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        records.push_back({ lvl, message });
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        records.push_back({ lvl, message });
    }
};

int info_notes(const std::vector<std::pair<meios::level, std::string>> &records)
{
    int count = 0;
    for(const std::pair<meios::level, std::string> &entry : records)
        if(entry.first == meios::level::info)
            ++count;
    return count;
}

bool any_contains(const std::vector<std::pair<meios::level, std::string>> &records,
                  std::string_view needle)
{
    for(const std::pair<meios::level, std::string> &entry : records)
        if(entry.second.find(needle) != std::string::npos)
            return true;
    return false;
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

TEST_CASE("substitution command dispatch resolves find, arg, eval and dirname",
          "[xacro][subst][cmd]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };

    std::filesystem::path root =
        std::filesystem::temp_directory_path() / "meios_subst_fixture";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "pkg");

    meios::directory_source on_disk{ root, log };
    meios::memory_source in_memory;
    in_memory.add("bytespkg", "", "resolved-from-bytes");
    meios::source_stack sources{ std::move(on_disk), std::move(in_memory) };

    meios::eval_scope scope;
    scope.set("width", meios::binding{ meios::value{ 0.3 } });
    std::filesystem::path document = root / "sub" / "robot.xacro";

    SECTION("$(find) resolves a package directory to a filesystem path")
    {
        meios::substitution out = meios::substitute("$(find pkg)/x.stl", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == std::filesystem::weakly_canonical(root / "pkg").string() + "/x.stl");
    }

    SECTION("$(find ${pkg}) resolves the inner expression before dispatch")
    {
        scope.set("pkgname", meios::binding{ std::string("pkg") });
        meios::substitution out =
            meios::substitute("$(find ${pkgname})/x.stl", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == std::filesystem::weakly_canonical(root / "pkg").string() + "/x.stl");
    }

    SECTION("$(find) refuses a bytes-only hit rather than naming a path that will not survive")
    {
        meios::substitution out = meios::substitute("$(find bytespkg)", scope, sources, document, log);
        REQUIRE_FALSE(out.ok);
        REQUIRE(any_contains(records, "byte-backed source"));
    }

    SECTION("an unresolved $(find) loud-fails")
    {
        meios::substitution out = meios::substitute("$(find missing)", scope, sources, document, log);
        REQUIRE_FALSE(out.ok);
        REQUIRE(any_contains(records, "did not resolve"));
    }

    SECTION("$(arg) returns the bound value")
    {
        meios::substitution out = meios::substitute("$(arg width)", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == "0.3");
    }

    SECTION("$(arg name default) falls back to the default when unset")
    {
        meios::substitution out = meios::substitute("$(arg height 0.5)", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == "0.5");
    }

    SECTION("$(arg name default) with a bound name never evaluates an unresolvable default")
    {
        scope.set("mesh_pkg", meios::binding{ std::string("pkg") });
        meios::substitution out =
            meios::substitute("$(arg mesh_pkg $(find absent_pkg))", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == "pkg");
        REQUIRE_FALSE(any_contains(records, "did not resolve"));
    }

    SECTION("an unset $(arg) with no default loud-fails")
    {
        meios::substitution out = meios::substitute("$(arg height)", scope, sources, document, log);
        REQUIRE_FALSE(out.ok);
        REQUIRE(any_contains(records, "unset and has no default"));
    }

    SECTION("$(eval) routes the remainder to the expression evaluator")
    {
        meios::substitution out = meios::substitute("$(eval 1 + 2)", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == "3");
    }

    SECTION("$(dirname) resolves the current document directory")
    {
        meios::substitution out = meios::substitute("$(dirname)/mesh", scope, sources, document, log);
        REQUIRE(out.ok);
        REQUIRE(out.text == document.parent_path().string() + "/mesh");
    }

    SECTION("an unknown command loud-fails")
    {
        meios::substitution out = meios::substitute("$(bogus x)", scope, sources, document, log);
        REQUIRE_FALSE(out.ok);
        REQUIRE(any_contains(records, "unknown substitution command"));
    }

    std::filesystem::remove_all(root);
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

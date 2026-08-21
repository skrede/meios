#include "xacro_subst_fixture.h"

#include <catch2/catch_test_macros.hpp>

namespace
{

struct scanned_span
{
    const char *raw;
    const char *subject;
};

// The span spellings whose inner text carries no span opener at all: the scan copies them
// byte for byte, so each must render exactly what it rendered before the inner scan existed.
const scanned_span untouched_spans[] = {
    { "${1 + 2}", "3" },
    { "${'a' + 'b'}", "ab" },
    { "${'price $5'}", "price $5" },
    { "${'a{b'}", "a{b" },
    { "${'a}b'}", "a}b" },
};

std::filesystem::path package_tree()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "meios_subst_order_fixture";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "pkg");
    return root;
}

}

TEST_CASE("an expression span resolves its inner text before the expression is evaluated",
          "[xacro][subst][order]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    const std::filesystem::path root = package_tree();
    meios::directory_source on_disk{ root, log };
    meios::source_stack sources{ std::move(on_disk) };
    meios::eval_scope scope;
    const std::filesystem::path document = root / "sub" / "robot.xacro";
    const std::string located = std::filesystem::weakly_canonical(root / "pkg").generic_string();

    SECTION("a package-locating command inside a quoted literal resolves to a path")
    {
        const substitution_result out =
            meios::substitute("${'$(find pkg)' + '/x.stl'}", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == located + "/x.stl");
    }

    SECTION("a property concatenated onto the located path is the composed spelling")
    {
        scope.set("tail", meios::value{ std::string("/meshes/base.dae") });
        const substitution_result out =
            meios::substitute("${'$(find pkg)' + tail}", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == located + "/meshes/base.dae");
    }

    SECTION("a nested expression span resolves before the outer expression is evaluated")
    {
        scope.set("inner", meios::value{ std::string("2 * 3") });
        const substitution_result out =
            meios::substitute("${${inner}}", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "6");
    }

    SECTION("an argument command with a bound name never evaluates its default inside a span")
    {
        scope.set("mesh_pkg", meios::value{ std::string("pkg") });
        const substitution_result out = meios::substitute(
            "${'$(arg mesh_pkg $(find absent_pkg))' + '/x.stl'}", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "pkg/x.stl");
        REQUIRE_FALSE(any_contains(records, "did not resolve"));
    }

    SECTION("a failed inner scan carries the inner span's own cause outward")
    {
        const substitution_result out =
            meios::substitute("${'$(find missing)' + '/x.stl'}", scope, sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::unresolved_find);
        REQUIRE(locates_a_file(out));
        REQUIRE(records_at(records, meios::level::error) == 1);
    }

    std::filesystem::remove_all(root);
}

TEST_CASE("a span whose inner text holds no opener is evaluated exactly as it was",
          "[xacro][subst][order]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;
    const std::filesystem::path document{ "robot.xacro" };

    for(const scanned_span &row : untouched_spans)
    {
        INFO(row.raw << " => " << row.subject);
        const substitution_result out = meios::substitute(row.raw, scope, sources, document, log);
        REQUIRE(out.has_value());
        CHECK(out->text == row.subject);
    }
}

// Measured against xacro 2.1.1: upstream truncates each of these at the first `}` and then
// refuses the fragment, where the quote- and depth-aware close finder here hands the evaluator
// the whole body and the grammar refuses it. Two refusals for different reasons, one verdict.
TEST_CASE("a brace-literal container and a format literal refuse where upstream refuses",
          "[xacro][subst][order]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;
    scope.set("v", *meios::value::make_real(1.2345));
    const std::filesystem::path document{ "robot.xacro" };

    SECTION("a brace-literal container refuses at the brace")
    {
        const substitution_result out =
            meios::substitute("${ {'a': 1} }", scope, sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(locates_a_file(out));
        REQUIRE(any_contains(records, "unsupported expression at '{'"));
    }

    SECTION("a format literal refuses as an unreadable token")
    {
        const substitution_result out =
            meios::substitute("${f'{v:.3f}'}", scope, sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(locates_a_file(out));
        REQUIRE(any_contains(records, "unexpected"));
    }
}

// On line 3 of the document below the outer `${` starts at 1-based column 19 and the nested one
// at 21. The nested scan refines the anchor onto its own span, so an outer failure reported at
// 21 would be the anchor the inner scan left behind rather than the span that failed.
TEST_CASE("a diagnostic raised after a nested scan reports the outer span's own column",
          "[xacro][subst][order]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;
    const std::string source =
        "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
        "  <xacro:property name=\"part\" value=\"'x'\"/>\n"
        "  <link name=\"pre ${${part} + missing}\"/>\n"
        "</robot>\n";

    const meios::expected<meios::expansion, meios::expansion_error> out =
        meios::expand(source, scope, sources, "robot.xacro", meios::expansion_limits{}, log);

    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::undefined_property);
    CHECK(out.error().loc.line == 3);
    CHECK(out.error().loc.column == 19);
    CHECK(any_contains(records, "'missing' is not defined"));
}

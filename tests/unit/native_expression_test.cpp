#include "oracle_records.h"
#include "native_expression_fixture.h"

#include "meios/xacro/lexer.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <string_view>

using native_expression_test::evaluate;
using native_expression_test::outcome;

namespace
{

std::vector<meios::detail::token_kind> kinds_of(std::string_view source)
{
    std::vector<meios::detail::token_kind> kinds;
    for(const meios::detail::token &one : meios::detail::tokenize(source))
        kinds.push_back(one.kind);
    return kinds;
}

}

TEST_CASE("a single-quoted literal scans as one string token carrying its text",
          "[native][expression]")
{
    const std::vector<meios::detail::token> tokens = meios::detail::tokenize("'continuous'");

    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].kind == meios::detail::token_kind::string);
    REQUIRE(tokens[0].text == "'continuous'");
    REQUIRE(tokens[0].leaf.text() == std::optional<std::string>("continuous"));
    REQUIRE(tokens[1].kind == meios::detail::token_kind::end);
}

TEST_CASE("the two unmeasured string spellings scan as refusals rather than as literals",
          "[native][expression]")
{
    REQUIRE(kinds_of("'text").front() == meios::detail::token_kind::error);
    REQUIRE(kinds_of("\"text\"").front() == meios::detail::token_kind::unsupported);
    REQUIRE(kinds_of("'a\\nb'").front() == meios::detail::token_kind::unsupported);
}

TEST_CASE("subscript, dotted access and membership each scan as their own kind",
          "[native][expression]")
{
    using kind = meios::detail::token_kind;

    REQUIRE(kinds_of("a[b]")
            == std::vector<kind>{ kind::name, kind::lbracket, kind::name, kind::rbracket,
                                  kind::end });
    REQUIRE(kinds_of("xacro.load_yaml")
            == std::vector<kind>{ kind::name, kind::dot, kind::name, kind::end });
    REQUIRE(kinds_of("pin in internal")
            == std::vector<kind>{ kind::name, kind::kw_in, kind::name, kind::end });
}

TEST_CASE("a decimal point inside a number is still part of the number", "[native][expression]")
{
    const std::vector<meios::detail::token> tokens = meios::detail::tokenize("1.5");

    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].kind == meios::detail::token_kind::number);
    REQUIRE(tokens[0].leaf.real() == std::optional<double>(1.5));
    REQUIRE(kinds_of(".5").front() == meios::detail::token_kind::number);
}

TEST_CASE("every measured expression evaluates to the value upstream produced",
          "[native][expression]")
{
    const std::vector<oracle::row> rows = oracle::load_rows("expressions.cases");
    REQUIRE_FALSE(rows.empty());

    std::size_t rendered = 0;
    for(const oracle::row &one : rows)
    {
        REQUIRE(one.fields.size() == 4);
        INFO("case " << one.fields[0] << ": " << one.fields[1]);
        const outcome ran = evaluate(one.fields[1]);
        if(one.fields[2] == "REFUSED")
        {
            REQUIRE_FALSE(one.fields[3].empty());
            REQUIRE(ran.failed);
            CHECK(oracle::relates(ran.messages, one.fields[3]));
            continue;
        }
        CHECK_FALSE(ran.failed);
        CHECK(ran.rendered == one.fields[2]);
        ++rendered;
    }
    // A record of nothing but refusals would otherwise reduce this case to the claim that the evaluator refuses everything, which measures nothing.
    REQUIRE(rendered * 4 >= rows.size() * 3);
}

TEST_CASE("a subscript chain reads by a literal key, by a variable key and through two levels",
          "[native][expression]")
{
    CHECK(evaluate("sec_mesh_files['base']['visual']['mesh']['package']").rendered
          == "ur_description");
    CHECK(evaluate("sec_mesh_files[name][type]['mesh']['path']").rendered
          == "meshes/base.dae");
    CHECK(evaluate("xacro.load_yaml(seed_file)['joint_limits']['shoulder_pan']['min']").rendered
          == "-6.28");
}

TEST_CASE("a subscript names what went wrong without naming what the mapping holds",
          "[native][expression]")
{
    const outcome absent = evaluate("sec_mesh_files['missing']");
    CHECK(absent.failed);
    REQUIRE(absent.messages.size() == 1);
    CHECK(absent.messages.front().find("missing") != std::string::npos);
    CHECK(absent.messages.front().find("base") == std::string::npos);

    CHECK(evaluate("mass['base']").messages.front().find("subscripted") != std::string::npos);
    CHECK(evaluate("sec_mesh_files[3]").messages.front().find("subscript key")
          != std::string::npos);
}

TEST_CASE("a string compares only against another string and never becomes a number",
          "[native][expression]")
{
    CHECK(evaluate("wrist_3_joint_type == 'continuous'").rendered == "True");
    CHECK(evaluate("'a' != 'b'").rendered == "True");
    CHECK(evaluate("wrist_3_joint_type != 3").rendered == "True");
    CHECK(evaluate("wrist_3_joint_type == 3").rendered == "False");
    CHECK(evaluate("'2' == 2").rendered == "False");

    const outcome ordered = evaluate("'a' < 'b'");
    CHECK(ordered.failed);
    CHECK(ordered.messages.front().find("ordering comparison") != std::string::npos);
}

// Repetition is real Python the CPython backend can run, so it stays in the one category a
// lenient policy may soften; a missing key or an unsubscriptable value stays terminal.
TEST_CASE("a string reaching the arithmetic it has no meaning for is unsupported, not a fault",
          "[native][expression]")
{
    CHECK(evaluate("wrist_3_joint_type + 'x'").rendered == "continuousx");
    CHECK(evaluate("wrist_3_joint_type * 2").kind == meios::eval_failure_kind::unsupported);
    CHECK(evaluate("wrist_3_joint_type + 2").kind == meios::eval_failure_kind::unsupported);
    CHECK(evaluate("sec_mesh_files['missing']").kind == meios::eval_failure_kind::error);
    CHECK(evaluate("sec_mesh_files + 1").kind == meios::eval_failure_kind::error);
}

TEST_CASE("membership tests a mapping's keys and refuses every other right-hand kind",
          "[native][expression]")
{
    CHECK(evaluate("type in sec_mesh_files[name]").rendered == "True");
    CHECK(evaluate("'absent' in sec_mesh_files").rendered == "False");

    const outcome scalar = evaluate("name in mass");
    CHECK(scalar.failed);
    CHECK(scalar.messages.front().find("membership") != std::string::npos);

    // Either order of a mixed run puts a boolean beside the membership test, and a boolean
    // is never a mapping key, so neither can quietly mean what Python's single comparison
    // level would have made of it.
    CHECK(evaluate("type in sec_mesh_files[name] == True").failed);
    CHECK(evaluate("type == 'visual' in sec_mesh_files").failed);
}

TEST_CASE("a construct outside the measured surface refuses by name", "[native][expression]")
{
    const std::pair<std::string_view, std::string_view> refused[] = {
        { "math.pi", "unsupported call target 'math.pi'" },
        { "mass.base", "not an auxiliary document" },
        { "list(1)", "the Python constructor 'list()'" },
        { "sec_mesh_files[1:2]", "unsupported subscript form" },
        { "[x for x in sec_mesh_files]", "unexpected 'for' in expression" },
        { "\"continuous\"", "unsupported expression at '\"'" },
    };

    for(const std::pair<std::string_view, std::string_view> &one : refused)
    {
        INFO("expression: " << one.first);
        const outcome ran = evaluate(one.first);
        CHECK(ran.failed);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find(one.second) != std::string::npos);
    }
}

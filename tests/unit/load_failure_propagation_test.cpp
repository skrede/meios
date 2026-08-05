#include "load_failure_fixture.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("an undefined property under eval_policy::fail returns a located error", "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("undefined_property.xacro"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::undefined_property);
    REQUIRE(result.error().loc.line > 0);
    REQUIRE(result.error().loc.column > 0);
}

TEST_CASE("an unresolvable $(find) in an attribute returns a located unresolved_find error",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("unresolvable_find_attr.xacro"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_find);
    REQUIRE(result.error().loc.line > 0);
    REQUIRE(result.error().loc.column > 0);
}

TEST_CASE("an unresolvable $(find) include under eval_policy::fail returns a located error",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("unresolvable_find_include.xacro"), opts);

    // The include seam extracts $(find ...) textually and reports the containing
    // include's failure to resolve the composed path, not the bare cmd_find code.
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_include);
    REQUIRE(result.error().loc.line > 0);
    REQUIRE(result.error().loc.column > 0);
}

TEST_CASE("the undefined-property error reports the failing token column, not the node anchor",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("undefined_property.xacro"), opts);

    // On line 3 `  <link name="${undeclared_link_name}"/>` the failing `${` starts at
    // 1-based column 15; the coarse node anchor would be column 4 (the element name).
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().loc.line == 3);
    REQUIRE(result.error().loc.column == 15);
}

TEST_CASE("an entity-before-token value degrades to a safe column, never a wrong offset",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("entity_before_token.xacro"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::undefined_property);
    REQUIRE(result.error().loc.line > 0);
    REQUIRE(result.error().loc.column > 0);

    // On line 3 `  <link name="pre&amp;${undeclared}"/>` the decode-replay maps the
    // failing token to raw column 23; a degrade may fall back to the value-start
    // column (15) or the node anchor (4). No other column is defensible — a value
    // outside this set would be a confidently-wrong offset.
    const int column = result.error().loc.column;
    const bool safe = column == 23 || column == 15 || column == 4;
    REQUIRE(safe);
}

TEST_CASE("eval_policy::warn keeps the specific evaluator code on a genuine error",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::warn;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("undefined_property.xacro"), opts);

    // An undefined name is a genuine evaluator error, not an unsupported construct,
    // so leniency does not leave it verbatim: the load still fails and the specific
    // code must survive the boundary replay rather than degrade to unspecified.
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::undefined_property);
}

TEST_CASE("a failed xacro load records no xml_parse_error cascade after the primary",
          "[urdf][load_failure]")
{
    std::vector<meios::captured_diagnostic> records;
    meios::log_sink_f sink{ diagnostic_recorder{ records } };

    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("undefined_property.xacro"), opts, sink);

    REQUIRE_FALSE(result.has_value());

    bool primary_present = false;
    int xml_parse_records = 0;
    for(const meios::captured_diagnostic &record : records)
    {
        if(record.code == meios::diagnostic_code::undefined_property)
            primary_present = true;
        if(record.code == meios::diagnostic_code::xml_parse_error)
            ++xml_parse_records;
    }
    REQUIRE(primary_present);
    REQUIRE(xml_parse_records == 0);
}

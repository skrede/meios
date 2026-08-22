#include "scalar_probe.h"
#include "oracle_records.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <string_view>

TEST_CASE("the recorded scalar measurements are present and non-empty", "[native][yaml]")
{
    const std::vector<oracle::row> rows = oracle::load_rows("yaml_scalars.cases");

    REQUIRE_FALSE(rows.empty());
    for(const oracle::row &one : rows)
        CHECK(one.fields.size() == 3);
}

TEST_CASE("the recorded unit-tag measurements are present and non-empty", "[native][yaml]")
{
    const std::vector<oracle::row> rows = oracle::load_rows("unit_tags.cases");

    REQUIRE_FALSE(rows.empty());
    for(const oracle::row &one : rows)
        CHECK(one.fields.size() == 3);
}

#ifdef MEIOS_TEST_HAS_YAML

TEST_CASE("every recorded scalar resolves to the kind and text upstream produced",
          "[native][yaml]")
{
    const std::vector<oracle::row> rows = oracle::load_rows("yaml_scalars.cases");
    REQUIRE_FALSE(rows.empty());

    std::size_t exercised = 0;
    for(const oracle::row &one : rows)
    {
        REQUIRE(one.fields.size() == 3);
        INFO("source [" << one.fields[0] << ']');
        const scalar_probe::outcome read = scalar_probe::resolve(one.fields[0]);
        REQUIRE(read.resolved.has_value());
        CHECK(scalar_probe::kind_column(read.resolved->kind()) == one.fields[1]);
        CHECK(meios::render_scalar(*read.resolved).value_or("<no scalar spelling>")
              == one.fields[2]);
        ++exercised;
    }

    CHECK(exercised == rows.size());
}

TEST_CASE("every recorded unit tag converts to the value upstream produced", "[native][yaml]")
{
    const std::vector<oracle::row> rows = oracle::load_rows("unit_tags.cases");
    REQUIRE_FALSE(rows.empty());

    std::size_t exercised = 0;
    for(const oracle::row &one : rows)
    {
        REQUIRE(one.fields.size() == 3);
        INFO("source [" << one.fields[0] << ' ' << one.fields[1] << ']');
        const scalar_probe::outcome read =
            scalar_probe::resolve(one.fields[0] + ' ' + one.fields[1]);
        REQUIRE(read.resolved.has_value());
        CHECK(read.resolved->kind() == meios::value_kind::real);
        CHECK(meios::render_scalar(*read.resolved).value_or("<no scalar spelling>")
              == one.fields[2]);
        ++exercised;
    }

    CHECK(exercised == rows.size());
}

TEST_CASE("a quoted scalar is text whatever it spells", "[native][yaml]")
{
    for(std::string_view quoted : { "'true'", "\"true\"", "'1'", "\"3.5\"", "'null'", "'~'" })
    {
        INFO("source [" << quoted << ']');
        const scalar_probe::outcome read = scalar_probe::resolve(std::string(quoted));
        REQUIRE(read.resolved.has_value());
        CHECK(read.resolved->kind() == meios::value_kind::string);
    }
}

TEST_CASE("a tagged value that is not a numeric literal refuses naming its own tag",
          "[native][yaml]")
{
    for(std::string_view spelling : { "!degrees pi/2", "!radians 2*90", "!meters half",
                                      "!millimeters ''", "!foot 0x10", "!inches sqrt(4)" })
    {
        INFO("source [" << spelling << ']');
        const scalar_probe::outcome refused = scalar_probe::resolve(std::string(spelling));
        CHECK_FALSE(refused.resolved.has_value());
        CHECK(refused.failure == meios::yaml_failure::unsupported);
        REQUIRE(refused.messages.size() == 1);
        CHECK(refused.messages.front().find(spelling.substr(0, spelling.find(' ')))
              != std::string::npos);
    }
}

// A tag outside the closed vocabulary is terminal rather than softenable: the fuller backend
// does not read it either, so retaining the span under a lenient policy would keep a document
// the reference loader rejects outright.
TEST_CASE("every other tag refuses by name", "[native][yaml]")
{
    for(std::string_view tag : { "!grams", "!kilograms", "!feet", "!inch", "!Degrees", "!!int" })
    {
        INFO("tag [" << tag << ']');
        const scalar_probe::outcome refused = scalar_probe::resolve(std::string(tag) + " 1");
        CHECK_FALSE(refused.resolved.has_value());
        CHECK(refused.failure == meios::yaml_failure::refused);
        REQUIRE(refused.messages.size() == 1);
        CHECK(refused.messages.front().find("unsupported tag") != std::string::npos);
    }
}

TEST_CASE("a number outside the value contract refuses rather than resolving", "[native][yaml]")
{
    for(std::string_view spelling : { ".inf", "-.inf", ".nan", "!degrees 1e400", "!inches 1e400" })
    {
        INFO("source [" << spelling << ']');
        const scalar_probe::outcome refused = scalar_probe::resolve(std::string(spelling));
        CHECK_FALSE(refused.resolved.has_value());
        CHECK(refused.messages.size() == 1);
        CHECK(refused.messages.front().find("finite") != std::string::npos);
    }
}

// PyYAML resolves a colon-separated number as a sexagesimal integer. No measurement authorizes
// that here, so it is named and refused rather than quietly read as text or as a number.
TEST_CASE("a sexagesimal number is refused rather than guessed", "[native][yaml]")
{
    const scalar_probe::outcome refused = scalar_probe::resolve("190:20:30");

    CHECK_FALSE(refused.resolved.has_value());
    CHECK(refused.messages.size() == 1);
    CHECK(refused.messages.front().find("sexagesimal") != std::string::npos);
}

#endif

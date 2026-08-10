#include "oracle_records.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <functional>
#include <string_view>

namespace
{

struct recorder
{
    std::vector<std::string> messages;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        (*this)(lvl, message);
    }
};

#ifdef MEIOS_TEST_HAS_YAML

// The record spells a floating-point result the way the upstream loader's own type is named;
// the value model calls the same kind real.
std::string kind_column(meios::value_kind kind)
{
    return kind == meios::value_kind::real ? "float" : std::string(meios::kind_name(kind));
}

struct probe
{
    std::optional<meios::value> resolved;
    meios::yaml_failure failure;
    std::vector<std::string> messages;
};

// Every probe goes through a whole document rather than through a private entry point, because
// the plain-versus-quoted distinction the record turns on exists only in the node stream.
probe resolve(const std::string &source)
{
    const std::shared_ptr<const meios::yaml_parser_handle> parser = meios::make_yaml_parser();
    const meios::evaluator_limits ceilings;
    meios::evaluator_counters counters;
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const std::string document = "probe: " + source + '\n';
    const meios::yaml_outcome out = (*parser)(document, ceilings, counters, sink, {});
    if(!out.parsed)
        return probe{ std::nullopt, out.failure, heard.messages };
    return probe{ out.parsed->at("probe"), out.failure, heard.messages };
}

#endif

}

TEST_CASE("the recorded scalar measurements are present and non-empty", "[native][yaml]")
{
    const std::vector<oracle::row> rows = oracle::load_rows("yaml_scalars.cases");

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
        INFO("source [" << one.fields[0] << ']');
        const probe read = resolve(one.fields[0]);
        REQUIRE(read.resolved.has_value());
        CHECK(kind_column(read.resolved->kind()) == one.fields[1]);
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
        const probe read = resolve(std::string(quoted));
        REQUIRE(read.resolved.has_value());
        CHECK(read.resolved->kind() == meios::value_kind::string);
    }
}

TEST_CASE("a tagged angle converts, and a tagged expression refuses", "[native][yaml]")
{
    const probe converted = resolve("!degrees 90");
    REQUIRE(converted.resolved.has_value());
    CHECK(converted.resolved->kind() == meios::value_kind::real);

    for(std::string_view spelling : { "!degrees pi/2", "!degrees 2*90", "!degrees half",
                                      "!degrees ''" })
    {
        INFO("source [" << spelling << ']');
        const probe refused = resolve(std::string(spelling));
        CHECK_FALSE(refused.resolved.has_value());
        CHECK(refused.messages.size() == 1);
        CHECK(refused.messages.front().find("!degrees") != std::string::npos);
    }
}

TEST_CASE("every other tag refuses by name", "[native][yaml]")
{
    for(std::string_view tag : { "!radians", "!meters", "!millimeters", "!inches", "!grams",
                                 "!kilograms", "!!int" })
    {
        INFO("tag [" << tag << ']');
        const probe refused = resolve(std::string(tag) + " 1");
        CHECK_FALSE(refused.resolved.has_value());
        CHECK(refused.failure == meios::yaml_failure::unsupported);
        CHECK(refused.messages.size() == 1);
        CHECK(refused.messages.front().find("unsupported tag") != std::string::npos);
    }
}

TEST_CASE("a number outside the value contract refuses rather than resolving", "[native][yaml]")
{
    for(std::string_view spelling : { ".inf", "-.inf", ".nan", "!degrees 1e400" })
    {
        INFO("source [" << spelling << ']');
        const probe refused = resolve(std::string(spelling));
        CHECK_FALSE(refused.resolved.has_value());
        CHECK(refused.messages.size() == 1);
        CHECK(refused.messages.front().find("finite") != std::string::npos);
    }
}

// PyYAML resolves a colon-separated number as a sexagesimal integer. No measurement authorizes
// that here, so it is named and refused rather than quietly read as text or as a number.
TEST_CASE("a sexagesimal number is refused rather than guessed", "[native][yaml]")
{
    const probe refused = resolve("190:20:30");

    CHECK_FALSE(refused.resolved.has_value());
    CHECK(refused.messages.size() == 1);
    CHECK(refused.messages.front().find("sexagesimal") != std::string::npos);
}

#endif

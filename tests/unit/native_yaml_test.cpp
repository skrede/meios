#include "native_yaml_probe.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <optional>
#include <string_view>

using namespace native_yaml_test;

TEST_CASE("default load options carry a parser exactly when the module is built", "[native][yaml]")
{
    const meios::load_options opts;

#ifdef MEIOS_HAS_YAML
    REQUIRE(opts.yaml);
    CHECK(opts.yaml->valid());
#else
    CHECK_FALSE(opts.yaml);
#endif
}

TEST_CASE("an explicitly chosen parser survives construction", "[native][yaml]")
{
    meios::load_options opts;
    const std::shared_ptr<const meios::yaml_parser_handle> empty = std::make_shared<const meios::yaml_parser_handle>();
    opts.yaml                                                    = empty;

    CHECK(opts.yaml == empty);
}

// The degradation the option governs, stated end to end and from the caller's side: options a
// caller constructs and does nothing else to carry whatever this build can do, and a build that
// can do nothing says so by name rather than failing to link or resolving to something invented.
TEST_CASE("a description reading an auxiliary document resolves or names what is missing", "[native][yaml]")
{
    const meios::load_options opts;
    const meios::evaluator_limits ceilings;
    const run out = substitute("a: 1\n", opts.yaml, ceilings, meios::eval_policy::fail);

#ifdef MEIOS_HAS_YAML
    CHECK(out.survived);
    CHECK(out.text == "value 1");
#else
    CHECK_FALSE(out.survived);
    REQUIRE_FALSE(out.messages.empty());
    CHECK(out.messages.front().find("resolves no auxiliary document format") != std::string::npos);
#endif
}

#ifdef MEIOS_TEST_HAS_YAML

TEST_CASE("the factory yields a parser the seam accepts", "[native][yaml]")
{
    const std::shared_ptr<const meios::yaml_parser_handle> parser = meios::make_yaml_parser();

    REQUIRE(parser);
    CHECK(parser->valid());
}

TEST_CASE("a mapping keeps its source order and reads by key", "[native][yaml]")
{
    const probe out = read("zulu: 1\nalpha: 2\nmike: 3\n");

    REQUIRE(out.parsed.has_value());
    REQUIRE(out.parsed->kind() == meios::value_kind::mapping);
    REQUIRE(out.parsed->size() == 3);
    CHECK(out.parsed->key_at(0) == "zulu");
    CHECK(out.parsed->key_at(1) == "alpha");
    CHECK(out.parsed->key_at(2) == "mike");
    CHECK(out.parsed->at("alpha")->integer().value_or(0) == 2);
}

TEST_CASE("a nested mapping converts through every level", "[native][yaml]")
{
    const probe out = read(nested_document);

    REQUIRE(out.parsed.has_value());
    const std::optional<meios::value> inner = out.parsed->at("a");
    REQUIRE(inner.has_value());
    REQUIRE(inner->at("b").has_value());
    CHECK(inner->at("b")->at("c")->integer().value_or(0) == 1);
    CHECK(out.counters.yaml_depth == 3);
    CHECK(out.counters.bytes == nested_document.size());
}

TEST_CASE("a construct outside the read surface refuses by name", "[native][yaml]")
{
    const std::pair<std::string_view, std::string_view> refusals[] = {
            {"? [1, 2]\n: 3\n", "a sequence as a mapping key"},
            {"? {a: 1}\n: 3\n", "a mapping as a mapping key"},
            {"a: !!set {}\n", "an unsupported tag"},
    };
    for(const std::pair<std::string_view, std::string_view> &one : refusals)
    {
        INFO("document [" << one.first << ']');
        const probe out = read(one.first);
        CHECK_FALSE(out.parsed.has_value());
        REQUIRE(out.messages.size() == 1);
        CHECK(out.messages.front().find(one.second) != std::string::npos);
        CHECK(out.messages.front().find("line") != std::string::npos);
    }
}

TEST_CASE("a key is read in any of the scalar kinds a document can write", "[native][yaml]")
{
    const std::pair<std::string_view, meios::value_kind> written[] = {
            {"null: n\n", meios::value_kind::null}, {"true: t\n", meios::value_kind::boolean}, {"1: i\n", meios::value_kind::integer},
            {"1.5: r\n", meios::value_kind::real},  {"'1': s\n", meios::value_kind::string},
    };
    for(const std::pair<std::string_view, meios::value_kind> &one : written)
    {
        INFO("document [" << one.first << ']');
        const probe out = read(one.first);
        REQUIRE(out.parsed.has_value());
        REQUIRE(out.parsed->size() == 1);
        CHECK(out.parsed->key_at(0)->kind() == one.second);
        CHECK(out.parsed->at(std::size_t{0}).has_value());
    }
}

TEST_CASE("every value a document produces reports its origin at every depth", "[native][yaml]")
{
    const probe out = read("a:\n  b:\n    c: 1\n  d:\n    - 2\n");

    REQUIRE(out.parsed.has_value());
    CHECK(out.parsed->from_yaml());
    CHECK(out.parsed->at("a")->from_yaml());
    CHECK(out.parsed->at("a")->at("b")->from_yaml());
    CHECK(out.parsed->at("a")->at("b")->at("c")->from_yaml());
    CHECK(out.parsed->at("a")->at("d")->from_yaml());
    CHECK(out.parsed->at("a")->at("d")->at(std::size_t{0})->from_yaml());
}

TEST_CASE("an empty document reads as a null that still reports its origin", "[native][yaml]")
{
    const probe out = read("");

    REQUIRE(out.parsed.has_value());
    CHECK(out.parsed->kind() == meios::value_kind::null);
    CHECK(out.parsed->from_yaml());
}

TEST_CASE("a child taken out of a document-built value carries the origin unaided", "[native][yaml]")
{
    const probe out = read("a:\n  - 1\n");

    REQUIRE(out.parsed.has_value());
    CHECK(out.parsed->at("a")->from_yaml());
    CHECK(out.parsed->at(std::size_t{0})->from_yaml());
    CHECK(out.parsed->at("a")->at(std::size_t{0})->from_yaml());
}

// Marking the whole tree is what makes the origin sound wherever a value is extracted, and the
// intended consequence is that a loaded mapping no longer equals the same mapping an author
// wrote: they disagree about where they came from, at the root and at every child.
TEST_CASE("a document-built mapping compares unequal to an identically shaped authored one", "[native][yaml]")
{
    const probe out = read("a: 1\n");
    std::vector<meios::value::entry> entries;
    entries.emplace_back("a", meios::value{std::int64_t{1}});
    const meios::value authored = meios::value::make_mapping(std::move(entries));

    REQUIRE(out.parsed.has_value());
    CHECK_FALSE(authored.from_yaml());
    CHECK_FALSE(authored.at("a")->from_yaml());
    CHECK_FALSE(*out.parsed == authored);
    CHECK_FALSE(*out.parsed == authored.with_yaml_origin());
}

#endif

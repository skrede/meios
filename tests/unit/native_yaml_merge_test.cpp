#include "document_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <string_view>

#ifdef MEIOS_TEST_HAS_YAML

namespace
{

constexpr std::string_view pair_source = "b: &b {x: 1, y: 2}\n";

document_probe::outcome merged(std::string_view document)
{
    return document_probe::read(std::string(pair_source) + std::string(document));
}

}

// Each shape and its entry set in key order, as the reference loader resolves them. Merged
// entries lead whatever the document wrote first, and an explicit entry keeps the merged key's
// position while replacing its value.
TEST_CASE("a merge key flattens to the entries the reference loader produces", "[native][yaml]")
{
    const std::pair<std::string_view, std::string_view> shapes[] = {
        { "d: {<<: *b, y: 9, z: 3}\n", "x=1,y=9,z=3" },
        { "d: {a0: 0, <<: *b, y: 9}\n", "x=1,y=9,a0=0" },
        { "d: {<<: *b, <<: *b}\n", "x=1,y=2" },
        { "d: {<<: {inline: 1}, y: 2}\n", "inline=1,y=2" },
        { "d: {<<: *b, x: 5}\n", "x=5,y=2" },
    };
    for(const std::pair<std::string_view, std::string_view> &one : shapes)
    {
        INFO("document [" << one.first << ']');
        const document_probe::outcome out = merged(one.first);
        REQUIRE(out.parsed.has_value());
        REQUIRE(out.parsed->at("d").has_value());
        CHECK(document_probe::entries_of(*out.parsed->at("d")) == one.second);
    }
}

// The sequence resolves in reverse element order, so the earlier element wins a conflict; both
// the ordering and the precedence it implies are asserted, because only one of the two is
// visible in a document whose merged mappings share no key.
TEST_CASE("a merge sequence resolves in reverse element order", "[native][yaml]")
{
    const document_probe::outcome apart = document_probe::read(
        "b1: &b1 {p: 1}\nb2: &b2 {q: 2}\nd: {z: 9, <<: [*b1, *b2]}\n");
    REQUIRE(apart.parsed.has_value());
    CHECK(document_probe::entries_of(*apart.parsed->at("d")) == "q=2,p=1,z=9");

    const document_probe::outcome shared = document_probe::read(
        "b1: &b1 {p: 1, r: 8}\nb2: &b2 {q: 2, r: 9}\nd: {<<: [*b1, *b2]}\n");
    REQUIRE(shared.parsed.has_value());
    CHECK(document_probe::entries_of(*shared.parsed->at("d")) == "q=2,r=8,p=1");
}

TEST_CASE("a merge of something other than a mapping refuses by name", "[native][yaml]")
{
    for(std::string_view document : { "d: {<<: 5}\n", "d: {<<: *b}\n" })
    {
        INFO("document [" << document << ']');
        const document_probe::outcome out =
            document_probe::read("b: &b [1, 2]\n" + std::string(document));
        CHECK_FALSE(out.parsed.has_value());
        REQUIRE(out.messages.size() == 1);
        CHECK(out.messages.front().find("a merge of a value that is not a mapping")
              != std::string::npos);
    }
}

// The quoted spelling is an ordinary text key: it is the non-specific tag, not the characters,
// that tells a merge from a key that happens to read the same.
TEST_CASE("a quoted merge spelling is an ordinary key", "[native][yaml]")
{
    const document_probe::outcome out = merged("d: {'<<': 5, y: 2}\n");

    REQUIRE(out.parsed.has_value());
    CHECK(document_probe::entries_of(*out.parsed->at("d")) == "<<=5,y=2");
}

// The mapping constructor already resolves a repeated key to its first position and its last
// value, so a document that writes one twice is not a refusal on either half.
TEST_CASE("a duplicate key resolves to the first position and the last value", "[native][yaml]")
{
    const document_probe::outcome written = document_probe::read("a: 1\nb: 0\na: 2\n");
    REQUIRE(written.parsed.has_value());
    CHECK(document_probe::entries_of(*written.parsed) == "a=2,b=0");

    const document_probe::outcome across = document_probe::read("1: one\ntrue: two\n");
    REQUIRE(across.parsed.has_value());
    CHECK(document_probe::entries_of(*across.parsed) == "1=two");
}

TEST_CASE("a key arriving through a merge resolves as one written twice does", "[native][yaml]")
{
    const document_probe::outcome through = merged("d: {<<: *b, x: 5}\n");
    const document_probe::outcome twice = document_probe::read("d: {x: 1, y: 2, x: 5}\n");

    REQUIRE(through.parsed.has_value());
    REQUIRE(twice.parsed.has_value());
    CHECK(*through.parsed->at("d") == *twice.parsed->at("d"));
}

TEST_CASE("an empty collection is a collection of no entries", "[native][yaml]")
{
    const document_probe::outcome out = document_probe::read("a: {}\nb: []\n");

    REQUIRE(out.parsed.has_value());
    CHECK(out.parsed->at("a")->kind() == meios::value_kind::mapping);
    CHECK(out.parsed->at("a")->size() == 0);
    CHECK(out.parsed->at("b")->kind() == meios::value_kind::sequence);
    CHECK(out.parsed->at("b")->size() == 0);
}

#endif

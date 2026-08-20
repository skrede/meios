#include "document_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

#ifdef MEIOS_TEST_HAS_YAML

namespace
{

// The shape the reference loader's amplification was measured on: one anchored sequence of nine
// leaves, then one anchored level per line, each aliasing the level below nine times. A level
// costs 46 bytes and nine alias events, and denotes nine times what the level below it does.
std::string amplified(std::size_t levels)
{
    std::string out = "l0: &l0 [x,x,x,x,x,x,x,x,x]\n";
    for(std::size_t at = 1; at <= levels; ++at)
    {
        const std::string below = "*l" + std::to_string(at - 1);
        out += "l" + std::to_string(at) + ": &l" + std::to_string(at) + " [" + below;
        for(std::size_t fan = 1; fan < 9; ++fan)
            out += ',' + below;
        out += "]\n";
    }
    return out;
}

}

TEST_CASE("an alias yields the value its anchor holds, whatever kind that is", "[native][yaml]")
{
    const std::string_view documents[] = { "a: &x {k: 1}\nb: *x\n", "a: &x [1, 2]\nb: *x\n",
                                           "a: &x 7\nb: *x\n", "a: &x ~\nb: *x\n" };
    for(std::string_view document : documents)
    {
        INFO("document [" << document << ']');
        const document_probe::outcome out = document_probe::read(document);
        REQUIRE(out.parsed.has_value());
        REQUIRE(out.parsed->at("b").has_value());
        CHECK(*out.parsed->at("b") == *out.parsed->at("a"));
    }
}

// Sharing rather than copying is what makes an aliased document cheap to build, and the
// auxiliary-node counter is where that shows: nine alias events charge nine nodes between them,
// not nine copies of the ten-node sequence they each denote.
TEST_CASE("an anchor aliased many times costs one node per alias", "[native][yaml]")
{
    const document_probe::outcome out = document_probe::read(amplified(1));

    REQUIRE(out.parsed.has_value());
    for(std::size_t at = 0; at < 9; ++at)
        CHECK(*out.parsed->at("l1")->at(at) == *out.parsed->at("l0"));
    CHECK(out.counters.yaml_nodes == 23);
    CHECK(out.counters.alias_expansion == 90);
}

// The measured levels, and the two numbers that make the axis necessary: the node ceiling
// charges events, of which the refused document raises far fewer than its default admits.
TEST_CASE("the alias axis bounds what a document denotes, not what it writes", "[native][yaml]")
{
    REQUIRE(amplified(5).size() == 258);
    REQUIRE(amplified(7).size() == 350);

    const document_probe::outcome admitted = document_probe::read(amplified(5));
    REQUIRE(admitted.parsed.has_value());
    CHECK(admitted.counters.alias_expansion == 672588);

    const document_probe::outcome refused = document_probe::read(amplified(7));
    CHECK_FALSE(refused.parsed.has_value());
    CHECK(refused.failure == meios::yaml_failure::exhausted);
    REQUIRE(refused.messages.size() == 1);
    CHECK(refused.messages.front().find("alias expansion") != std::string::npos);
    CHECK(refused.counters.yaml_nodes < meios::default_yaml_node_ceiling);
}

TEST_CASE("the alias ceiling admits the level below the one it refuses", "[native][yaml]")
{
    CHECK_FALSE(document_probe::read(amplified(6)).parsed.has_value());
    CHECK(document_probe::read(amplified(5)).parsed.has_value());
}

// A self-referential graph loads upstream as a value that contains itself. An immutable value
// with shared collection storage cannot denote one, so the alias arrives while its anchor is
// still being built and is refused there.
TEST_CASE("an alias to an anchor that is not yet complete refuses by name", "[native][yaml]")
{
    for(std::string_view document : { "a: &a [1, *a]\n", "a: &a {k: *a}\n", "&a [*a]\n" })
    {
        INFO("document [" << document << ']');
        const document_probe::outcome out = document_probe::read(document);
        CHECK_FALSE(out.parsed.has_value());
        CHECK(out.failure == meios::yaml_failure::refused);
        REQUIRE(out.messages.size() == 1);
        CHECK(out.messages.front().find("not yet complete") != std::string::npos);
        CHECK(out.messages.front().find("line") != std::string::npos);
    }
}

// An anchor a document writes on a key is an anchor like any other, so the table is filled from
// the key path too rather than only from the value path.
TEST_CASE("a key carries an anchor an alias can name", "[native][yaml]")
{
    const document_probe::outcome out = document_probe::read("? &k a\n: 1\nb: *k\n");

    REQUIRE(out.parsed.has_value());
    CHECK(document_probe::entries_of(*out.parsed) == "a=1,b=a");
}

TEST_CASE("an alias standing where a key belongs refuses on what it holds", "[native][yaml]")
{
    const document_probe::outcome scalar = document_probe::read("a: &x 7\n? *x\n: read\n");
    REQUIRE(scalar.parsed.has_value());
    CHECK(document_probe::entries_of(*scalar.parsed) == "a=7,7=read");

    const document_probe::outcome listed = document_probe::read("a: &x [1]\n? *x\n: read\n");
    CHECK_FALSE(listed.parsed.has_value());
    REQUIRE(listed.messages.size() == 1);
    CHECK(listed.messages.front().find("a sequence as a mapping key") != std::string::npos);
}

#endif

#include "string_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <string_view>

TEST_CASE("the named split yields the fields between separators, the empty ones included",
          "[native][string]")
{
    const meios::value fields = strings::evaluated("'a b c'.split(' ')");

    REQUIRE(fields.kind() == meios::value_kind::sequence);
    REQUIRE(fields.size() == 3);
    CHECK(strings::joined(fields) == "a|b|c");
    CHECK(strings::joined(strings::evaluated("'a  b'.split(' ')")) == "a||b");
    CHECK(strings::joined(strings::evaluated("' a '.split(' ')")) == "|a|");
    CHECK(strings::joined(strings::evaluated("''.split(' ')")).empty());
    CHECK(strings::evaluated("''.split(' ')").size() == 1);
    CHECK(strings::joined(strings::evaluated("'abc'.split('x')")) == "abc");
    CHECK(strings::joined(strings::evaluated("'ab'.split('b')")) == "a|");
    CHECK(strings::joined(strings::evaluated("'a--b'.split('--')")) == "a|b");
}

TEST_CASE("a split result is read by the subscript layer and by nothing of its own",
          "[native][string]")
{
    CHECK(strings::evaluate("'a b c'.split(' ')[0]").rendered == "a");
    CHECK(strings::evaluate("'a b c'.split(' ')[2]").rendered == "c");
    CHECK(strings::evaluate("'a b c'.split(' ')[-1]").rendered == "c");
    CHECK(strings::evaluate("'a b c'.split(' ')[-3]").rendered == "a");
    CHECK(strings::evaluate("fields[1]").rendered == "0.2");

    const strings::outcome past = strings::evaluate("'a b c'.split(' ')[3]");
    CHECK(past.failed);
    REQUIRE_FALSE(past.messages.empty());
    CHECK(past.messages.front().find("index 3 is outside the sequence") != std::string::npos);
    CHECK(strings::evaluate("'a b'.split(' ')['0']").failed);
}

TEST_CASE("the split refuses every argument form but the one separator", "[native][string]")
{
    const std::pair<std::string_view, std::string_view> refused[] = {
        { "'a b'.split()", "split with no separator" },
        { "'a b'.split(' ', 1)", "split takes one separator" },
        { "'a b'.split(1)", "the separator must be text" },
        { "'a b'.split(table)", "the separator must be text" },
        { "'a b'.split('')", "an empty separator" }
    };

    for(const std::pair<std::string_view, std::string_view> &one : refused)
    {
        INFO("expression: " << one.first);
        const strings::outcome ran = strings::evaluate(std::string(one.first));
        CHECK(ran.failed);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find(one.second) != std::string::npos);
    }
}

// A base that is not a string never reaches the named operation, and each refuses by the gate it
// was already standing at: a loaded sequence by its kind, a loaded mapping by the absent key --
// which is what upstream's own wrapper answers there -- and everything else at the origin gate.
TEST_CASE("the named operation reaches no base but a string", "[native][string]")
{
    const std::pair<std::string_view, std::string_view> refused[] = {
        { "loaded.split(' ')", "a sequence has no member 'split'" },
        { "config.split(' ')", "key 'split' is not present" },
        { "mass.split(' ')", "not an auxiliary document" },
        { "table.split(' ')", "not an auxiliary document" },
        { "fields.split(' ')", "not an auxiliary document" }
    };

    for(const std::pair<std::string_view, std::string_view> &one : refused)
    {
        INFO("expression: " << one.first);
        const strings::outcome ran = strings::evaluate(std::string(one.first));
        CHECK(ran.failed);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find(one.second) != std::string::npos);
    }
}

TEST_CASE("no other member spelling on a string yields anything at all", "[native][string]")
{
    for(std::string_view form : { "'abc'.upper()", "'abc'.strip()", "'abc'.replace('a', 'b')",
                                  "'abc'.join(fields)", "'abc'.format()", "'abc'.upper",
                                  "'abc'.split", "prefix.__class__" })
    {
        INFO("expression: " << form);
        const strings::outcome ran = strings::evaluate(std::string(form));
        CHECK(ran.failed);
        CHECK(ran.kind == meios::eval_failure_kind::unsupported);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find("answers no member") != std::string::npos);
    }
}

// The operation is on text, so the origin mark that gates a mapping's member path does not
// reach it: a string a document produced splits exactly as one an author wrote.
TEST_CASE("a document-originated string splits as an authored one does", "[native][string]")
{
    CHECK(strings::evaluate("config['path'].split('/')[-1]").rendered == "base.dae");
    CHECK(strings::evaluate("config['path'].split('/')[0]").rendered == "meshes");
    CHECK(strings::joined(strings::evaluated("config['xyz'].split(' ')")) == "0.1|0.2|0.3");
    CHECK(strings::evaluate("config['path'].upper()").failed);
}

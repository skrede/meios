#include "construct_probe.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <optional>
#include <algorithm>
#include <string_view>

using construct_probe::load_result;
using construct_probe::listed;
using construct_probe::named;
using construct_probe::read;

namespace
{

using names = std::vector<std::string_view>;

struct row
{
    std::string_view expression;
    std::string_view auxiliary;
    names exercised;
};

// Each row is the smallest document that reaches its construct at all, so what it reports beyond
// that construct is what reaching it costs rather than what the row happens to carry: a sequence
// subscript needs a sequence to stand on, and a negative one needs a subscript.
std::vector<row> expression_rows()
{
    return { { "dict(a=1)['a']", "", { "mapping-literal" } },
             { "'a' + 'b'", "", { "string-concatenation" } },
             { "'a' in 'abc'", "", { "string-membership" } },
             { "not 'x'", "", { "string-truth" } },
             { "'a b'.split(' ')", "", { "named-split" } } };
}

std::vector<row> document_rows()
{
    return { { "xacro.load_yaml('aux.yaml')['b']", "a:\n  - 1\nb: 2\n", { "document-sequence" } },
             { "xacro.load_yaml('aux.yaml')['a']", "a: &x 1\nb: *x\n", { "alias" } },
             { "xacro.load_yaml('aux.yaml')['a']['p']", "a: {<<: {p: 1}}\n", { "merge-key" } },
             { "xacro.load_yaml('aux.yaml')['a']", "a: 1\na: 2\n", { "duplicate-key" } },
             { "xacro.load_yaml('aux.yaml')['b']", "1: x\nb: 2\n", { "non-string-key" } },
             { "xacro.load_yaml('aux.yaml')['a']", "a: !degrees 90\n", { "unit-tag" } },
             { "xacro.load_yaml('aux.yaml').b", "b: 2\n", { "dotted-member" } },
             { "xacro.load_yaml('aux.yaml')['a'][0]", "a:\n  - 1\n  - 2\n",
               { "document-sequence", "sequence-subscript" } },
             { "xacro.load_yaml('aux.yaml')['a'][-1]", "a:\n  - 1\n  - 2\n",
               { "document-sequence", "sequence-subscript", "negative-index" } } };
}

std::string joined(const names &spellings)
{
    std::string out;
    for(std::string_view one : spellings)
    {
        out += out.empty() ? "" : ", ";
        out += one;
    }
    return out;
}

names spellings_of(const std::vector<row> &rows)
{
    names out;
    for(const row &one : rows)
        for(std::string_view spelling : one.exercised)
            if(std::ranges::find(out, spelling) == out.end())
                out.push_back(spelling);
    return out;
}

}

TEST_CASE("the vocabulary pairs every member with exactly one spelling", "[native][constructs]")
{
    for(const meios::construct_spelling &one : meios::construct_spellings)
    {
        INFO(one.name);
        CHECK(meios::construct_name(one.construct) == one.name);
        CHECK(meios::construct_from_name(one.name) == std::optional{ one.construct });
    }
    CHECK(meios::evaluator_construct_count == 14);
}

TEST_CASE("a spelling the vocabulary does not carry resolves to nothing", "[native][constructs]")
{
    for(std::string_view absent : { "", "sequence", "document_sequence", "DOCUMENT-SEQUENCE",
                                    "slice", "brace-literal", "alias " })
    {
        INFO(absent);
        CHECK(meios::construct_from_name(absent) == std::nullopt);
    }
}

TEST_CASE("a load exercising nothing in the vocabulary reports nothing", "[native][constructs]")
{
    const load_result got = read("1 + 1");

    CHECK(got.ok);
    CHECK(got.exercised.empty());
    CHECK(named(got.exercised).empty());
}

TEST_CASE("each construct the expression grammar reaches is observed by a load that reaches it",
          "[native][constructs]")
{
    for(const row &one : expression_rows())
    {
        INFO(one.expression);
        const load_result got = read(one.expression);
        CHECK(got.ok);
        CHECK(listed(got.exercised) == joined(one.exercised));
    }
}

#ifdef MEIOS_TEST_HAS_YAML

TEST_CASE("each construct an auxiliary document reaches is observed by a load that reaches it",
          "[native][constructs][yaml]")
{
    const std::shared_ptr<const meios::yaml_parser_handle> parser = meios::make_yaml_parser();

    for(const row &one : document_rows())
    {
        INFO(one.expression << " over " << one.auxiliary);
        const load_result got = read(one.expression, one.auxiliary, parser);
        CHECK(got.ok);
        CHECK(listed(got.exercised) == joined(one.exercised));
    }
}

// What keeps the two tables above from silently falling behind the vocabulary they are checked
// against: a member added with nothing exercising it fails here rather than passing unnoticed.
TEST_CASE("every member of the vocabulary is reached by a case", "[native][constructs][yaml]")
{
    names reached = spellings_of(expression_rows());
    for(std::string_view spelling : spellings_of(document_rows()))
        reached.push_back(spelling);

    for(const meios::construct_spelling &one : meios::construct_spellings)
    {
        INFO(one.name);
        CHECK(std::ranges::find(reached, one.name) != reached.end());
    }
    CHECK(reached.size() == meios::evaluator_construct_count);
}

// The observation is per load or it is nothing: two loads sharing the parser a caller installs
// once must each report what they themselves exercised.
TEST_CASE("two loads driven from one configuration report their own sets",
          "[native][constructs][yaml]")
{
    const std::shared_ptr<const meios::yaml_parser_handle> parser = meios::make_yaml_parser();

    SECTION("one after the other")
    {
        const load_result first =
            read("xacro.load_yaml('aux.yaml')['a']", "a: &x 1\nb: *x\n", parser);
        const load_result second =
            read("xacro.load_yaml('aux.yaml')['a']", "a: !degrees 90\n", parser);

        CHECK(listed(first.exercised) == "alias");
        CHECK(listed(second.exercised) == "unit-tag");
    }

    SECTION("at the same time")
    {
        load_result first{};
        load_result second{};
        std::thread one([&] {
            first = read("xacro.load_yaml('aux.yaml')['a'][-1]", "a:\n  - 1\n  - 2\n", parser);
        });
        std::thread two([&] { second = read("dict(a=1)['a']", "", parser); });
        one.join();
        two.join();

        CHECK(listed(first.exercised) == "document-sequence, sequence-subscript, negative-index");
        CHECK(listed(second.exercised) == "mapping-literal");
    }
}

#endif

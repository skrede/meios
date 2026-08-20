#include "oracle_records.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <functional>
#include <string_view>

namespace
{

#ifdef MEIOS_TEST_HAS_YAML

struct heard
{
    std::vector<std::string> messages;
    std::vector<meios::diagnostic_code> codes;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &message)
    {
        if(lvl == meios::level::error)
            codes.push_back(code);
        (*this)(lvl, message);
    }
};

struct probe
{
    bool failed;
    meios::eval_failure_kind kind;
    std::string rendered;
    std::vector<std::string> messages;
    std::vector<meios::diagnostic_code> codes;
};

meios::value parsed(std::string_view document, heard &into)
{
    meios::log_sink_f sink{ std::ref(into) };
    meios::evaluator_counters counters;
    const meios::yaml_outcome out =
        (*meios::make_yaml_parser())(document, meios::evaluator_limits{}, counters, sink, {});
    return out.parsed.value_or(meios::value{});
}

// The subject is always the document's own root bound to one name, so an expression under test
// reads a value the installed parser produced rather than one the test assembled.
probe evaluate(std::string_view document, std::string_view expression)
{
    heard into;
    meios::log_sink_f sink{ std::ref(into) };
    meios::eval_scope scope;
    scope.set("items", parsed(document, into));
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return probe{ evaluator.failed(), evaluator.failure_kind(),
                  meios::render_scalar(result).value_or(std::string()), into.messages, into.codes };
}

constexpr std::string_view three = "- 1\n- 2\n- 3\n";

bool names(const std::vector<std::string> &messages, std::string_view what)
{
    for(const std::string &one : messages)
        if(one.find(what) != std::string::npos)
            return true;
    return false;
}

#endif

}

#ifdef MEIOS_TEST_HAS_YAML

TEST_CASE("a sequence reads forwards by position and backwards from the end, and an empty one "
          "has no position at all",
          "[native][sequence]")
{
    const std::pair<std::string_view, std::string_view> reads[] = {
        { "items[0]", "1" },  { "items[1]", "2" },  { "items[2]", "3" },
        { "items[-1]", "3" }, { "items[-2]", "2" }, { "items[-3]", "1" }
    };
    for(const std::pair<std::string_view, std::string_view> &one : reads)
    {
        INFO("expression " << one.first);
        const probe out = evaluate(three, one.first);
        CHECK_FALSE(out.failed);
        CHECK(out.rendered == one.second);
    }
    heard into;
    const meios::value empty = parsed("[]\n", into);
    REQUIRE(empty.kind() == meios::value_kind::sequence);
    CHECK(empty.size() == 0);
    CHECK(evaluate("[]\n", "items[0]").failed);
    CHECK(evaluate("[]\n", "items[-1]").failed);
}

TEST_CASE("an index outside the sequence refuses naming the index and no element",
          "[native][sequence]")
{
    for(std::string_view expression : { "items[3]", "items[-4]" })
    {
        INFO("expression " << expression);
        const probe out = evaluate(three, expression);
        REQUIRE(out.failed);
        CHECK(out.kind == meios::eval_failure_kind::error);
        REQUIRE_FALSE(out.codes.empty());
        CHECK(out.codes.front() == meios::diagnostic_code::undefined_property);
    }
    const probe wide = evaluate("- 1234567\n- 7654321\n", "items[5]");
    REQUIRE(wide.failed);
    CHECK(names(wide.messages, "5"));
    CHECK_FALSE(names(wide.messages, "1234567"));
    CHECK_FALSE(names(wide.messages, "7654321"));
}

TEST_CASE("a wrong-kind subscript names the kind it was offered", "[native][sequence]")
{
    const probe text_key = evaluate(three, "items['0']");
    REQUIRE(text_key.failed);
    CHECK(text_key.kind == meios::eval_failure_kind::error);
    CHECK(names(text_key.messages, "string"));

    const probe position_key = evaluate("a: 1\n", "items[0]");
    REQUIRE(position_key.failed);
    CHECK(position_key.kind == meios::eval_failure_kind::error);
    CHECK(names(position_key.messages, "integer"));
}

TEST_CASE("a sequence in key position and a tagged sequence each refuse typed",
          "[native][sequence]")
{
    heard key_position;
    parsed("? [1, 2]\n: 3\n", key_position);
    REQUIRE(key_position.messages.size() == 1);
    CHECK(names(key_position.messages, "a sequence as a mapping key"));
    CHECK(names(key_position.messages, "line"));

    heard tagged;
    parsed("!custom [1, 2]\n", tagged);
    REQUIRE(tagged.messages.size() == 1);
    CHECK(names(tagged.messages, "!custom"));
    CHECK(names(tagged.messages, "unsupported tag"));
}

TEST_CASE("a sequence nests inside a sequence and inside a mapping value", "[native][sequence]")
{
    const probe nested = evaluate("- [1, 2]\n- 3\n", "items[0][1]");
    CHECK_FALSE(nested.failed);
    CHECK(nested.rendered == "2");

    const probe held = evaluate("limits:\n  - 4\n  - 5\n", "items['limits'][-1]");
    CHECK_FALSE(held.failed);
    CHECK(held.rendered == "5");
}

// The expectation is read out of the record rather than written here: the spelling refuses
// because upstream refused it, and the diagnostic has to name what upstream named.
TEST_CASE("each recorded subscript refusal refuses here and names the same thing",
          "[native][sequence]")
{
    const std::vector<oracle::row> rows = oracle::load_rows("expressions.cases");
    const std::pair<std::string_view, std::string_view> recorded[] = {
        { "s03", "items[3]" }, { "s04", "items[-4]" }, { "s05", "items['0']" }
    };
    for(const std::pair<std::string_view, std::string_view> &one : recorded)
    {
        INFO("case " << one.first);
        const std::optional<oracle::row> row = oracle::lookup(rows, one.first);
        REQUIRE(row.has_value());
        REQUIRE(row->fields.size() == 4);
        REQUIRE(row->fields[2] == "REFUSED");
        const probe out = evaluate(three, one.second);
        CHECK(out.failed);
        CHECK(oracle::relates(out.messages, row->fields[3]));
    }
}

#endif

#include "meios/xacro/eval_parser.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <optional>
#include <functional>
#include <string_view>

namespace
{

using key = meios::detail::scalar_key;

meios::value integer_value(std::int64_t number)
{
    return meios::value{ number };
}

meios::value real_value(double number)
{
    const std::optional<meios::value> made = meios::value::make_real(number);
    REQUIRE(made.has_value());
    return *made;
}

meios::value string_value(std::string text)
{
    return meios::value{ std::move(text) };
}

meios::value sample_mapping()
{
    std::vector<meios::value::entry> entries;
    entries.emplace_back("zulu", integer_value(1));
    entries.emplace_back("alpha", integer_value(2));
    entries.emplace_back("mike", integer_value(3));
    return meios::value::make_mapping(std::move(entries));
}

struct tally
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

std::string document(std::string_view body)
{
    return "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">" + std::string(body)
         + "</robot>";
}

std::vector<meios::value> all_kinds()
{
    std::vector<meios::value> every;
    every.emplace_back();
    every.push_back(meios::value{ true });
    every.push_back(integer_value(7));
    every.push_back(real_value(1.5));
    every.push_back(string_value("arm"));
    every.push_back(meios::value::make_sequence({ integer_value(1) }));
    every.push_back(sample_mapping());
    return every;
}

}

TEST_CASE("a default value is null and renders as Python's None", "[native][value]")
{
    const meios::value none;

    REQUIRE(none.kind() == meios::value_kind::null);
    REQUIRE(meios::render_scalar(none) == std::optional<std::string>("None"));
    REQUIRE_FALSE(none.boolean().has_value());
    REQUIRE(none.size() == 0);
}

TEST_CASE("a boolean renders the way Python spells it", "[native][value]")
{
    REQUIRE(meios::render_scalar(meios::value{ true }) == std::optional<std::string>("True"));
    REQUIRE(meios::render_scalar(meios::value{ false }) == std::optional<std::string>("False"));
    REQUIRE(meios::value{ true }.boolean() == std::optional<bool>(true));
    REQUIRE(meios::value{ true }.kind() == meios::value_kind::boolean);
}

TEST_CASE("an integer round-trips the whole fixed-width range", "[native][value]")
{
    const std::int64_t lowest = INT64_MIN;
    const std::int64_t highest = INT64_MAX;

    REQUIRE(integer_value(lowest).integer() == std::optional<std::int64_t>(lowest));
    REQUIRE(integer_value(highest).integer() == std::optional<std::int64_t>(highest));
    REQUIRE(meios::render_scalar(integer_value(highest))
            == std::optional<std::string>("9223372036854775807"));
}

TEST_CASE("a real renders through the shared double printer and refuses a non-finite",
          "[native][value]")
{
    REQUIRE(meios::render_scalar(real_value(0.5)) == std::optional<std::string>("0.5"));
    REQUIRE(meios::render_scalar(real_value(2.0)) == std::optional<std::string>("2.0"));
    REQUIRE(real_value(0.5).kind() == meios::value_kind::real);

    REQUIRE_FALSE(meios::value::make_real(std::numeric_limits<double>::infinity()).has_value());
    REQUIRE_FALSE(meios::value::make_real(-std::numeric_limits<double>::infinity()).has_value());
    REQUIRE_FALSE(meios::value::make_real(std::numeric_limits<double>::quiet_NaN()).has_value());
}

TEST_CASE("text that reads as a boolean is still a string", "[native][value]")
{
    const meios::value spelled = string_value("True");

    REQUIRE(spelled.kind() == meios::value_kind::string);
    REQUIRE_FALSE(spelled.boolean().has_value());
    REQUIRE(meios::render_scalar(spelled) == std::optional<std::string>("True"));
}

TEST_CASE("a sequence keeps construction order and reports its size", "[native][value]")
{
    const meios::value items =
        meios::value::make_sequence({ integer_value(3), integer_value(1), integer_value(2) });

    REQUIRE(items.kind() == meios::value_kind::sequence);
    REQUIRE(items.size() == 3);
    REQUIRE(items.at(std::size_t{ 0 }) == std::optional<meios::value>(integer_value(3)));
    REQUIRE(items.at(std::size_t{ 2 }) == std::optional<meios::value>(integer_value(2)));
    REQUIRE_FALSE(items.at(std::size_t{ 3 }).has_value());
    REQUIRE_FALSE(meios::render_scalar(items).has_value());
}

TEST_CASE("a mapping iterates in source order and looks up regardless of it", "[native][value]")
{
    const meios::value table = sample_mapping();

    REQUIRE(table.kind() == meios::value_kind::mapping);
    REQUIRE(table.size() == 3);
    REQUIRE(table.key_at(0) == key{ "zulu" });
    REQUIRE(table.key_at(1) == key{ "alpha" });
    REQUIRE(table.key_at(2) == key{ "mike" });

    REQUIRE(table.at("alpha") == std::optional<meios::value>(integer_value(2)));
    REQUIRE(table.at("zulu") == std::optional<meios::value>(integer_value(1)));
    REQUIRE_FALSE(table.at("absent").has_value());
    REQUIRE_FALSE(meios::render_scalar(table).has_value());
}

TEST_CASE("a duplicate mapping key keeps the last value in the first position", "[native][value]")
{
    std::vector<meios::value::entry> entries;
    entries.emplace_back("dof", integer_value(6));
    entries.emplace_back("name", string_value("arm"));
    entries.emplace_back("dof", integer_value(7));
    const meios::value table = meios::value::make_mapping(std::move(entries));

    REQUIRE(table.size() == 2);
    REQUIRE(table.key_at(0) == key{ "dof" });
    REQUIRE(table.at("dof") == std::optional<meios::value>(integer_value(7)));
}

TEST_CASE("a key compares the way Python compares the key object", "[native][value]")
{
    CHECK(key{ "dof" } == key{ "dof" });
    CHECK_FALSE(key{ "dof" } == key{ "name" });
    CHECK(key{} == key{});
    CHECK_FALSE(key{} == key{ std::string() });
    CHECK_FALSE(key{} == key{ "null" });
    CHECK(key{ std::int64_t{ 1 } } == key{ true });
    CHECK(key{ std::int64_t{ 0 } } == key{ false });
    CHECK(key{ std::int64_t{ 1 } } == key{ 1.0 });
    CHECK(key{ true } == key{ 1.0 });
    CHECK_FALSE(key{ std::int64_t{ 1 } } == key{ "1" });
}

// A bare int64_t == double would convert the integer to double first and lose every
// distinction above 2^53, folding two keys a document wrote apart into one.
TEST_CASE("an integer key compares against a real without losing precision", "[native][value]")
{
    CHECK(key{ std::int64_t{ 9007199254740992 } } == key{ 9007199254740992.0 });
    CHECK_FALSE(key{ std::int64_t{ 9007199254740993 } } == key{ 9007199254740992.0 });
    CHECK_FALSE(key{ INT64_MAX } == key{ 9223372036854775808.0 });
    CHECK_FALSE(key{ std::int64_t{ 1 } } == key{ 1.5 });
}

TEST_CASE("a key written in a second scalar kind resolves the way upstream resolves it",
          "[native][value]")
{
    struct collision
    {
        std::size_t entries;
        meios::value_kind kept;
        std::string_view shape;
        key first;
        key second;
    };
    const collision measured[] = {
        { 1, meios::value_kind::integer, "1 then true", key{ std::int64_t{ 1 } }, key{ true } },
        { 1, meios::value_kind::integer, "1 then 1.0", key{ std::int64_t{ 1 } }, key{ 1.0 } },
        { 1, meios::value_kind::boolean, "true then 1", key{ true }, key{ std::int64_t{ 1 } } },
        { 1, meios::value_kind::integer, "0 then false", key{ std::int64_t{ 0 } }, key{ false } },
        { 2, meios::value_kind::null, "null then 'null'", key{}, key{ "null" } },
        { 2, meios::value_kind::string, "'1' then 1", key{ "1" }, key{ std::int64_t{ 1 } } },
    };
    for(const collision &one : measured)
    {
        INFO("keys [" << one.shape << ']');
        std::vector<meios::value::entry> entries;
        entries.emplace_back(one.first, string_value("a"));
        entries.emplace_back(one.second, string_value("b"));
        const meios::value table = meios::value::make_mapping(std::move(entries));

        REQUIRE(table.size() == one.entries);
        CHECK(table.key_at(0)->kind() == one.kept);
        CHECK(table.at(std::size_t{ 0 })
              == std::optional<meios::value>(string_value(one.entries == 1 ? "b" : "a")));
    }
}

TEST_CASE("a lookup by text finds the text key and not a number spelled the same",
          "[native][value]")
{
    std::vector<meios::value::entry> entries;
    entries.emplace_back(key{ "1" }, string_value("quoted"));
    entries.emplace_back(key{ std::int64_t{ 1 } }, string_value("plain"));
    const meios::value table = meios::value::make_mapping(std::move(entries));

    REQUIRE(table.size() == 2);
    CHECK(table.at("1") == std::optional<meios::value>(string_value("quoted")));
    CHECK(table.at(key{ std::int64_t{ 1 } }) == std::optional<meios::value>(string_value("plain")));
    CHECK(table.at(key{ true }) == std::optional<meios::value>(string_value("plain")));
}

TEST_CASE("keyed lookup is total over every kind", "[native][value]")
{
    for(const meios::value &subject : all_kinds())
    {
        if(subject.kind() == meios::value_kind::mapping)
            continue;
        REQUIRE_FALSE(subject.at("anything").has_value());
    }
}

TEST_CASE("the arithmetic helpers are total over every kind", "[native][value]")
{
    for(const meios::value &subject : all_kinds())
    {
        const bool arithmetic = subject.kind() == meios::value_kind::boolean
                             || subject.kind() == meios::value_kind::integer
                             || subject.kind() == meios::value_kind::real;
        REQUIRE(meios::detail::as_int(subject).has_value() == arithmetic);
        REQUIRE(meios::detail::as_double(subject).has_value() == arithmetic);
        REQUIRE(meios::detail::truthy(subject).has_value()
                == (arithmetic || subject.kind() == meios::value_kind::null
                    || subject.kind() == meios::value_kind::string));
        REQUIRE(meios::detail::is_int(subject)
                == (subject.kind() == meios::value_kind::boolean
                    || subject.kind() == meios::value_kind::integer));
    }
}

TEST_CASE("an arithmetic-meaningless kind fails typed instead of throwing", "[native][value]")
{
    meios::log_sink silent;
    meios::eval_scope scope;
    scope.set("table", sample_mapping());
    scope.set("name", string_value("arm"));

    meios::core_evaluator table_use;
    table_use.eval("table + 1", scope, silent);
    REQUIRE(table_use.failed());

    meios::core_evaluator string_use;
    string_use.eval("name + 1", scope, silent);
    REQUIRE(string_use.failed());
}

TEST_CASE("a value survives a copy, a move and a scope round trip", "[native][value]")
{
    const meios::value origin = sample_mapping().with_yaml_origin();
    REQUIRE(origin.from_yaml());

    meios::value copied = origin;
    const meios::value moved = std::move(copied);
    REQUIRE(moved == origin);
    REQUIRE(moved.from_yaml());

    meios::eval_scope scope;
    scope.set("config", moved);
    const std::optional<meios::value> read_back = scope.lookup("config");
    REQUIRE(read_back.has_value());
    REQUIRE(*read_back == origin);
    REQUIRE(read_back->from_yaml());

    const meios::value plain = sample_mapping();
    REQUIRE_FALSE(plain.from_yaml());
    REQUIRE_FALSE(plain == origin);
}

TEST_CASE("a mapping crosses a property, a macro and a nested scope intact", "[native][value]")
{
    meios::eval_scope scope;
    scope.set("config", sample_mapping().with_yaml_origin());
    meios::source_stack sources;
    meios::log_sink silent;

    const std::string source = document(
        "<xacro:property name=\"copy\" value=\"${config}\"/>"
        "<xacro:macro name=\"m\" params=\"table:=${config}\">"
        "<xacro:property name=\"inner\" value=\"${table}\" scope=\"global\"/>"
        "</xacro:macro><xacro:m/>");
    const meios::expected<meios::expansion, meios::expansion_error> expanded =
        meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, silent);

    REQUIRE(expanded.has_value());
    REQUIRE(scope.lookup("copy") == std::optional<meios::value>(sample_mapping().with_yaml_origin()));
    REQUIRE(scope.lookup("inner") == std::optional<meios::value>(sample_mapping().with_yaml_origin()));
    REQUIRE(scope.lookup("copy")->from_yaml());
}

TEST_CASE("an exact substitution keeps the evaluated type where mixed text renders it",
          "[native][value]")
{
    meios::eval_scope scope;
    meios::source_stack sources;
    meios::log_sink silent;

    const std::string source = document(
        "<xacro:property name=\"exact\" value=\"${1+1}\"/>"
        "<xacro:property name=\"mixed\" value=\"n${1+1}\"/>"
        "<xacro:property name=\"real\" value=\"${1/2}\"/>");
    REQUIRE(meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, silent)
                .has_value());

    REQUIRE(scope.lookup("exact")->kind() == meios::value_kind::integer);
    REQUIRE(scope.lookup("exact")->integer() == std::optional<std::int64_t>(2));
    REQUIRE(scope.lookup("mixed")->kind() == meios::value_kind::string);
    REQUIRE(scope.lookup("mixed")->text() == std::optional<std::string>("n2"));
    REQUIRE(scope.lookup("real")->kind() == meios::value_kind::real);
}

TEST_CASE("a collection in mixed text fails typed and names no content", "[native][value]")
{
    meios::eval_scope scope;
    scope.set("config", sample_mapping());
    meios::source_stack sources;
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };

    const meios::expected<meios::expansion, meios::expansion_error> expanded =
        meios::expand(document("<l>name ${config}</l>"), scope, sources, "inline.xacro",
                      meios::expansion_limits{}, sink);

    REQUIRE_FALSE(expanded.has_value());
    REQUIRE(expanded.error().code == meios::diagnostic_code::expression_error);
    REQUIRE_FALSE(counts.messages.empty());
    REQUIRE(counts.messages.front().find("config") != std::string::npos);
    REQUIRE(counts.messages.front().find("mapping") != std::string::npos);
    REQUIRE(counts.messages.front().find("zulu") == std::string::npos);
}

#include "native_value_probe.h"

#include "meios/xacro/eval_parser.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <optional>
#include <string_view>

using namespace native_value_test;

using key = meios::detail::scalar_key;

TEST_CASE("a key compares the way Python compares the key object", "[native][value]")
{
    CHECK(key{"dof"} == key{"dof"});
    CHECK_FALSE(key{"dof"} == key{"name"});
    CHECK(key{} == key{});
    CHECK_FALSE(key{} == key{std::string()});
    CHECK_FALSE(key{} == key{"null"});
    CHECK(key{std::int64_t{1}} == key{true});
    CHECK(key{std::int64_t{0}} == key{false});
    CHECK(key{std::int64_t{1}} == key{1.0});
    CHECK(key{true} == key{1.0});
    CHECK_FALSE(key{std::int64_t{1}} == key{"1"});
}

// A bare int64_t == double would convert the integer to double first and lose every
// distinction above 2^53, folding two keys a document wrote apart into one.
TEST_CASE("an integer key compares against a real without losing precision", "[native][value]")
{
    CHECK(key{std::int64_t{9007199254740992}} == key{9007199254740992.0});
    CHECK_FALSE(key{std::int64_t{9007199254740993}} == key{9007199254740992.0});
    CHECK_FALSE(key{INT64_MAX} == key{9223372036854775808.0});
    CHECK_FALSE(key{std::int64_t{1}} == key{1.5});
}

TEST_CASE("a key written in a second scalar kind resolves the way upstream resolves it", "[native][value]")
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
            {1, meios::value_kind::integer, "1 then true", key{std::int64_t{1}}, key{true}},
            {1, meios::value_kind::integer, "1 then 1.0", key{std::int64_t{1}}, key{1.0}},
            {1, meios::value_kind::boolean, "true then 1", key{true}, key{std::int64_t{1}}},
            {1, meios::value_kind::integer, "0 then false", key{std::int64_t{0}}, key{false}},
            {2, meios::value_kind::null, "null then 'null'", key{}, key{"null"}},
            {2, meios::value_kind::string, "'1' then 1", key{"1"}, key{std::int64_t{1}}},
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
        CHECK(table.at(std::size_t{0}) == std::optional<meios::value>(string_value(one.entries == 1 ? "b" : "a")));
    }
}

TEST_CASE("a lookup by text finds the text key and not a number spelled the same", "[native][value]")
{
    std::vector<meios::value::entry> entries;
    entries.emplace_back(key{"1"}, string_value("quoted"));
    entries.emplace_back(key{std::int64_t{1}}, string_value("plain"));
    const meios::value table = meios::value::make_mapping(std::move(entries));

    REQUIRE(table.size() == 2);
    CHECK(table.at("1") == std::optional<meios::value>(string_value("quoted")));
    CHECK(table.at(key{std::int64_t{1}}) == std::optional<meios::value>(string_value("plain")));
    CHECK(table.at(key{true}) == std::optional<meios::value>(string_value("plain")));
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
        const bool arithmetic = subject.kind() == meios::value_kind::boolean || subject.kind() == meios::value_kind::integer || subject.kind() == meios::value_kind::real;
        REQUIRE(meios::detail::as_int(subject).has_value() == arithmetic);
        REQUIRE(meios::detail::as_double(subject).has_value() == arithmetic);
        REQUIRE(meios::detail::truthy(subject).has_value() == (arithmetic || subject.kind() == meios::value_kind::null || subject.kind() == meios::value_kind::string));
        REQUIRE(meios::detail::is_int(subject) == (subject.kind() == meios::value_kind::boolean || subject.kind() == meios::value_kind::integer));
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

#include "native_value_probe.h"

#include "meios/xacro/eval_parser.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <optional>

using namespace native_value_test;

using key = meios::detail::scalar_key;

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
    REQUIRE(meios::render_scalar(meios::value{true}) == std::optional<std::string>("True"));
    REQUIRE(meios::render_scalar(meios::value{false}) == std::optional<std::string>("False"));
    REQUIRE(meios::value{true}.boolean() == std::optional<bool>(true));
    REQUIRE(meios::value{true}.kind() == meios::value_kind::boolean);
}

TEST_CASE("an integer round-trips the whole fixed-width range", "[native][value]")
{
    const std::int64_t lowest  = INT64_MIN;
    const std::int64_t highest = INT64_MAX;

    REQUIRE(integer_value(lowest).integer() == std::optional<std::int64_t>(lowest));
    REQUIRE(integer_value(highest).integer() == std::optional<std::int64_t>(highest));
    REQUIRE(meios::render_scalar(integer_value(highest)) == std::optional<std::string>("9223372036854775807"));
}

TEST_CASE("a real renders through the shared double printer and refuses a non-finite", "[native][value]")
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
    const meios::value items = meios::value::make_sequence({integer_value(3), integer_value(1), integer_value(2)});

    REQUIRE(items.kind() == meios::value_kind::sequence);
    REQUIRE(items.size() == 3);
    REQUIRE(items.at(std::size_t{0}) == std::optional<meios::value>(integer_value(3)));
    REQUIRE(items.at(std::size_t{2}) == std::optional<meios::value>(integer_value(2)));
    REQUIRE_FALSE(items.at(std::size_t{3}).has_value());
    REQUIRE_FALSE(meios::render_scalar(items).has_value());
}

TEST_CASE("a mapping iterates in source order and looks up regardless of it", "[native][value]")
{
    const meios::value table = sample_mapping();

    REQUIRE(table.kind() == meios::value_kind::mapping);
    REQUIRE(table.size() == 3);
    REQUIRE(table.key_at(0) == key{"zulu"});
    REQUIRE(table.key_at(1) == key{"alpha"});
    REQUIRE(table.key_at(2) == key{"mike"});

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
    REQUIRE(table.key_at(0) == key{"dof"});
    REQUIRE(table.at("dof") == std::optional<meios::value>(integer_value(7)));
}

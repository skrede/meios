#include "oracle_records.h"

#include "meios/xacro/reference_names.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <string_view>

TEST_CASE("the built-in names are the ones the reference's table holds, in its order", "[xacro][root_symbols]")
{
    const std::vector<oracle::row> rows = oracle::load_rows("global_symbols.cases");
    REQUIRE(rows.size() == std::size(meios::detail::root_symbols));
    for(std::size_t i = 0; i < rows.size(); ++i)
    {
        INFO("row " << i);
        REQUIRE(rows[i].fields.size() == 1);
        CHECK(rows[i].fields[0] == std::string(meios::detail::root_symbols[i]));
    }
}

TEST_CASE("every mathematics name the evaluator answers is a built-in name of the reference", "[xacro][root_symbols]")
{
    for(std::string_view name : meios::detail::mathematics_names)
    {
        INFO(name);
        CHECK(meios::detail::listed(meios::detail::root_symbols, name));
    }
}

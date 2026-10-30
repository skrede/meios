#include "meios/xacro/boolean_value.h"

#include <meios/xacro/value.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstdint>
#include <optional>

using meios::detail::python_int;
using meios::detail::get_boolean_value;

namespace
{

std::optional<bool> spelled(const char *text)
{
    return get_boolean_value(meios::value{ std::string(text) });
}

}

TEST_CASE("a spelling the reference reads as true reads true", "[xacro][boolean]")
{
    for(const char *text : {"true", "True", "1", " 1", "+1", "1_0"})
    {
        DYNAMIC_SECTION(text)
        {
            REQUIRE(spelled(text) == std::optional<bool>(true));
        }
    }
}

TEST_CASE("a spelling the reference reads as false reads false", "[xacro][boolean]")
{
    for(const char *text : {"false", "False", "0", "-0"})
    {
        DYNAMIC_SECTION(text)
        {
            REQUIRE(spelled(text) == std::optional<bool>(false));
        }
    }
}

TEST_CASE("a spelling the reference refuses answers no boolean", "[xacro][boolean]")
{
    for(const char *text : {"1.0", "yes", "TRUE", "", "1__0"})
    {
        DYNAMIC_SECTION(text)
        {
            REQUIRE_FALSE(spelled(text).has_value());
        }
    }
}

TEST_CASE("a typed value reads by its truth", "[xacro][boolean]")
{
    REQUIRE(get_boolean_value(meios::value{ true }) == std::optional<bool>(true));
    REQUIRE(get_boolean_value(meios::value{ std::int64_t{ 2 } }) == std::optional<bool>(true));
    REQUIRE(get_boolean_value(*meios::value::make_real(1.0)) == std::optional<bool>(true));
    REQUIRE(get_boolean_value(*meios::value::make_real(0.0)) == std::optional<bool>(false));
    REQUIRE(get_boolean_value(meios::value{}) == std::optional<bool>(false));
    REQUIRE(get_boolean_value(meios::value::make_sequence({})) == std::optional<bool>(false));
}

TEST_CASE("the integer reader keeps what int() reads and refuses what it raises on", "[xacro][boolean]")
{
    REQUIRE(python_int("\t-0_07 \n") == std::optional<std::string>("-7"));
    REQUIRE(python_int("000") == std::optional<std::string>("0"));
    REQUIRE(python_int("99999999999999999999999") == std::optional<std::string>("99999999999999999999999"));
    for(const char *text : {"_1", "1_", "- 1", "+-1", "0x1", " ", "\x1c" "1"})
    {
        DYNAMIC_SECTION(text)
        {
            REQUIRE_FALSE(python_int(text).has_value());
        }
    }
}

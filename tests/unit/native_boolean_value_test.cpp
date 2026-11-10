#include "meios/xacro/boolean_value.h"
#include "meios/xacro/python_number.h"

#include <meios/xacro/value.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

using meios::detail::python_int;
using meios::detail::python_real;
using meios::detail::get_boolean_value;

namespace
{

std::optional<bool> spelled(const char *text)
{
    return get_boolean_value(meios::value{ std::string(text) });
}

}

TEST_CASE("true, True or a nonzero integer spelling reads true", "[xacro][boolean]")
{
    for(const char *text : {"true", "True", "1", " 1", "+1", "1_0", "١"})
    {
        DYNAMIC_SECTION(text)
        {
            REQUIRE(spelled(text) == std::optional<bool>(true));
        }
    }
}

TEST_CASE("false, False or a zero integer spelling reads false", "[xacro][boolean]")
{
    for(const char *text : {"false", "False", "0", "-0"})
    {
        DYNAMIC_SECTION(text)
        {
            REQUIRE(spelled(text) == std::optional<bool>(false));
        }
    }
}

TEST_CASE("a finite real spelling reads by whether it is nonzero", "[xacro][boolean]")
{
    for(const char *text : {"2.5", "1e3", ".5", "1.0", "-1.5"})
    {
        DYNAMIC_SECTION(text)
        {
            REQUIRE(spelled(text) == std::optional<bool>(true));
        }
    }
    for(const char *text : {"0.0", "-0.0", "0e5"})
    {
        DYNAMIC_SECTION(text)
        {
            REQUIRE(spelled(text) == std::optional<bool>(false));
        }
    }
}

TEST_CASE("any other spelling, a non-finite real among them, answers no boolean", "[xacro][boolean]")
{
    for(const char *text : {"nan", "inf", "-Infinity", "TRUE", "yes", "0x10", "", "1__0"})
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
    std::vector<meios::value::entry> entries;
    entries.emplace_back("k", meios::value{ std::int64_t{ 0 } });
    REQUIRE(get_boolean_value(meios::value::make_mapping(std::move(entries))) == std::optional<bool>(true));
}

TEST_CASE("the integer reader keeps what int() reads and refuses what it raises on", "[xacro][boolean]")
{
    REQUIRE(python_int("\t-0_07 \n") == std::optional<std::string>("-7"));
    REQUIRE(python_int("000") == std::optional<std::string>("0"));
    REQUIRE(python_int("99999999999999999999999") == std::optional<std::string>("99999999999999999999999"));
    REQUIRE(python_int("١٢") == std::optional<std::string>("12"));
    REQUIRE(python_int("  +7 ") == std::optional<std::string>("7"));
    REQUIRE(python_int(" 1　") == std::optional<std::string>("1"));
    for(const char *text : {"_1", "1_", "1__0", "- 1", "+-1", "0x1", " ", "\x1c" "1"})
    {
        DYNAMIC_SECTION(text)
        {
            REQUIRE_FALSE(python_int(text).has_value());
        }
    }
}

TEST_CASE("the real reader keeps what float() reads and refuses what it raises on", "[xacro][boolean]")
{
    REQUIRE(python_real("1_0.5") == std::optional<double>(10.5));
    REQUIRE(python_real(" 5. ") == std::optional<double>(5.0));
    REQUIRE(python_real("-.5e-1") == std::optional<double>(-0.05));
    REQUIRE(python_real("١.٢e١") == std::optional<double>(12.0));
    REQUIRE(python_real("+Infinity") == std::optional<double>(std::numeric_limits<double>::infinity()));
    REQUIRE(python_real("1e400") == std::optional<double>(std::numeric_limits<double>::infinity()));
    REQUIRE(python_real("-1e-400") == std::optional<double>(0.0));
    REQUIRE(std::signbit(*python_real("-1e-400")));
    REQUIRE(std::isnan(*python_real("-nAn")));
    for(const char *text : {".", "1e", "e3", "1_e5", "1e_5", "1._5", "infin", "1j", "\x1c" "1.5"})
    {
        DYNAMIC_SECTION(text)
        {
            REQUIRE_FALSE(python_real(text).has_value());
        }
    }
}

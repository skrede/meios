#ifndef HPP_GUARD_MEIOS_TEST_NATIVE_VALUE_PROBE_H
#define HPP_GUARD_MEIOS_TEST_NATIVE_VALUE_PROBE_H

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <optional>
#include <string_view>

namespace native_value_test
{

inline meios::value integer_value(std::int64_t number)
{
    return meios::value{number};
}

inline meios::value real_value(double number)
{
    const std::optional<meios::value> made = meios::value::make_real(number);
    REQUIRE(made.has_value());
    return *made;
}

inline meios::value string_value(std::string text)
{
    return meios::value{std::move(text)};
}

inline meios::value sample_mapping()
{
    std::vector<meios::value::entry> entries;
    entries.emplace_back("zulu", integer_value(1));
    entries.emplace_back("alpha", integer_value(2));
    entries.emplace_back("mike", integer_value(3));
    return meios::value::make_mapping(std::move(entries));
}

inline std::vector<meios::value> all_kinds()
{
    std::vector<meios::value> every;
    every.emplace_back();
    every.push_back(meios::value{true});
    every.push_back(integer_value(7));
    every.push_back(real_value(1.5));
    every.push_back(string_value("arm"));
    every.push_back(meios::value::make_sequence({integer_value(1)}));
    every.push_back(sample_mapping());
    return every;
}

inline std::string document(std::string_view body)
{
    return "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">" + std::string(body) + "</robot>";
}

}

#endif

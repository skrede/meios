#include <meios/version.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>

TEST_CASE("version() is a dotted release string", "[integration]")
{
    const std::string_view reported = meios::version();
    REQUIRE_FALSE(reported.empty());
    REQUIRE(std::count(reported.begin(), reported.end(), '.') == 2);
}

#include <meios/version.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("version() reports the CMake project version", "[version]")
{
    REQUIRE(meios::version() == MEIOS_CMAKE_PROJECT_VERSION);
}

#include <meios/version.h>
#include <meios/detail/xml_engine.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("version() reports the CMake project version", "[version]")
{
    REQUIRE(meios::version() == MEIOS_CMAKE_PROJECT_VERSION);
}

TEST_CASE("the pugixml link anchor resolves against the core archive", "[version]")
{
    REQUIRE(meios::detail::xml_engine_ready());
}

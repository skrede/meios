#include <meios/urdf.h>
#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

static_assert(meios::format_reader<meios::urdf_reader>);
static_assert(meios::model_sink<meios::world_recorder>);

TEST_CASE("the meios::urdf umbrella header include-compiles", "[urdf][smoke]")
{
    REQUIRE(true);
}

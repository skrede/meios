#include <meios/bundle.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("emit_result carries the status and the asset counters", "[bundle][pkg]")
{
    const meios::emit_result result{ meios::emit_status::ok, 0, 0 };

    REQUIRE(result.status == meios::emit_status::ok);
    REQUIRE(result.assets_seen == 0);
    REQUIRE(result.unresolved == 0);
}

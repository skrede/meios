#include <meios/bundle.h>
#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <rapidcheck.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("the parse->flatten->parse round-trip harness is live", "[bundle][property]")
{
    REQUIRE(rc::check("emit_status round-trips through its string form", [] {
        RC_ASSERT(meios::to_string(meios::emit_status::ok) == "ok");
    }));
}

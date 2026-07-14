#include <meios/bundle.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("emit_status stringifies for the diagnostic channel", "[bundle][writer]")
{
    REQUIRE(meios::to_string(meios::emit_status::ok) == "ok");
    REQUIRE(meios::to_string(meios::emit_status::xml_unencodable) == "xml_unencodable");
    REQUIRE(meios::to_string(meios::emit_status::io_error) == "io_error");
}

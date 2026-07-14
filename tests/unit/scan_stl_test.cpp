#include <meios/scan/stl_scanner.h>

#include <meios/bundle.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>

namespace
{

meios::resolved_asset make_asset(const char *name)
{
    return meios::resolved_asset{ std::filesystem::path{ name } };
}

}

static_assert(meios::asset_scanner<meios::stl_scanner>);

TEST_CASE("an stl scan reports no external references", "[scan][stl]")
{
    meios::stl_scanner scanner;
    meios::log_sink silent;
    meios::resolved_asset asset = make_asset("part.stl");

    REQUIRE(scanner.scan(asset, silent).empty());
}

TEST_CASE("a registered stl scanner is a known, empty-yielding extension", "[scan][stl]")
{
    meios::scanner_registry registry;
    meios::log_sink silent;
    meios::resolved_asset asset = make_asset("part.stl");

    REQUIRE_FALSE(registry.contains("stl"));
    meios::register_stl_scanner(registry);

    REQUIRE(registry.contains("stl"));
    REQUIRE(registry.scan("stl", asset, silent).empty());
}

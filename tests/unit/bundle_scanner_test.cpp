#include <meios/bundle.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <filesystem>
#include <type_traits>

namespace
{

struct obj_scanner
{
    std::vector<std::string> refs;

    std::vector<std::string> scan(const meios::resolved_asset &, meios::log_sink &) { return refs; }
};

meios::resolved_asset make_asset(std::string name)
{
    return meios::resolved_asset{ std::filesystem::path{ std::move(name) } };
}

}

static_assert(meios::asset_scanner<obj_scanner>);
static_assert(!std::is_copy_constructible_v<meios::scanner_handle>);
static_assert(std::is_move_constructible_v<meios::scanner_handle>);

TEST_CASE("an unknown extension returns the null default", "[bundle][scanner]")
{
    meios::scanner_registry registry;
    meios::log_sink silent;
    meios::resolved_asset asset = make_asset("thing.xyz");

    REQUIRE(registry.scan("xyz", asset, silent).empty());
    REQUIRE(registry.empty());
}

TEST_CASE("a registered scanner is dispatched by its lowercased extension", "[bundle][scanner]")
{
    meios::scanner_registry registry;
    registry.register_scanner("obj", meios::scanner_handle{ obj_scanner{ { "a.mtl", "b.mtl" } } });
    meios::log_sink silent;
    meios::resolved_asset asset = make_asset("mesh.obj");

    const std::vector<std::string> refs = registry.scan("OBJ", asset, silent);

    REQUIRE(registry.contains("obj"));
    REQUIRE(refs == std::vector<std::string>{ "a.mtl", "b.mtl" });
}

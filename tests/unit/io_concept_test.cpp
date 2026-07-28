#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <string_view>
#include <type_traits>

namespace
{

struct plain_source
{
    meios::capability_descriptor capabilities() const
    {
        return { meios::source_kind::memory, false };
    }

    std::optional<meios::resolved_asset> locate(std::string_view, std::string_view)
    {
        return std::nullopt;
    }
};

struct path_source
{
    meios::capability_descriptor capabilities() const
    {
        return { meios::source_kind::directory, false };
    }

    std::optional<meios::resolved_asset> locate(std::string_view, std::string_view)
    {
        return std::nullopt;
    }

    std::optional<std::filesystem::path> path_of(std::string_view, std::string_view) const
    {
        return std::filesystem::path{ "pkg/model.urdf" };
    }
};

struct enumerating_source
{
    meios::capability_descriptor capabilities() const
    {
        return { meios::source_kind::directory, true };
    }

    std::optional<meios::resolved_asset> locate(std::string_view, std::string_view)
    {
        return std::nullopt;
    }

    std::vector<std::string> packages() const { return { "pkg_a", "pkg_b" }; }
};

}

static_assert(meios::package_source<plain_source>);
static_assert(meios::package_source<path_source>);
static_assert(meios::package_source<enumerating_source>);

static_assert(!meios::provides_path<plain_source>);
static_assert(meios::provides_path<path_source>);
static_assert(!meios::provides_path<enumerating_source>);

static_assert(!meios::enumerates_packages<plain_source>);
static_assert(!meios::enumerates_packages<path_source>);
static_assert(meios::enumerates_packages<enumerating_source>);

static_assert(std::is_copy_constructible_v<meios::resolved_asset>);
static_assert(std::is_copy_assignable_v<meios::resolved_asset>);

TEST_CASE("resolved_asset reads back the path it was built from", "[io][concept]")
{
    const meios::resolved_asset asset{ std::filesystem::path{ "share/robot.urdf" } };

    REQUIRE(asset.path() == std::filesystem::path{ "share/robot.urdf" });
}

TEST_CASE("a copied resolved_asset names the same path as its original", "[io][concept]")
{
    const meios::resolved_asset asset{ std::filesystem::path{ "a/b.dae" } };
    const meios::resolved_asset copy{ asset };

    REQUIRE(copy.path() == asset.path());
    REQUIRE(copy.path() == std::filesystem::path{ "a/b.dae" });
}

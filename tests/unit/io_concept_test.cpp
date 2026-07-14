#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <type_traits>

namespace
{

struct string_puller final : meios::byte_reader::puller
{
    std::string data;
    std::size_t pos{ 0 };

    explicit string_puller(std::string bytes) : data(std::move(bytes)) {}

    std::size_t read(std::span<std::byte> out) override
    {
        std::size_t n = std::min(out.size(), data.size() - pos);
        std::memcpy(out.data(), data.data() + pos, n);
        pos += n;
        return n;
    }
};

meios::byte_reader make_reader(std::string bytes)
{
    return meios::byte_reader{ std::make_unique<string_puller>(std::move(bytes)) };
}

struct plain_source
{
    meios::capability_descriptor capabilities() const
    {
        return { meios::source_kind::memory, false, false };
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
        return { meios::source_kind::directory, true, false };
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
        return { meios::source_kind::directory, false, true };
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

static_assert(!std::is_copy_constructible_v<meios::resolved_asset>);
static_assert(!std::is_copy_assignable_v<meios::resolved_asset>);
static_assert(std::is_move_constructible_v<meios::resolved_asset>);
static_assert(std::is_move_assignable_v<meios::resolved_asset>);

static_assert(!std::is_copy_constructible_v<meios::byte_reader>);
static_assert(std::is_move_constructible_v<meios::byte_reader>);

TEST_CASE("resolved_asset built from a path holds the path alternative", "[io][concept]")
{
    meios::resolved_asset asset{ std::filesystem::path{ "share/robot.urdf" } };

    REQUIRE(asset.holds_path());
    REQUIRE_FALSE(asset.holds_bytes());
    REQUIRE(asset.path() == std::filesystem::path{ "share/robot.urdf" });
}

TEST_CASE("resolved_asset built from a byte_reader reads its bytes back", "[io][concept]")
{
    meios::resolved_asset asset{ make_reader("mesh-bytes") };

    REQUIRE(asset.holds_bytes());
    REQUIRE_FALSE(asset.holds_path());

    std::array<std::byte, 4> buffer{};
    std::string round_trip;
    for(std::size_t n = asset.bytes().read(buffer); n != 0; n = asset.bytes().read(buffer))
        round_trip.append(reinterpret_cast<const char *>(buffer.data()), n);

    REQUIRE(round_trip == "mesh-bytes");
}

TEST_CASE("resolved_asset is move-only and transfers its content on move", "[io][concept]")
{
    meios::resolved_asset asset{ std::filesystem::path{ "a/b.dae" } };
    meios::resolved_asset moved{ std::move(asset) };

    REQUIRE(moved.holds_path());
    REQUIRE(moved.path() == std::filesystem::path{ "a/b.dae" });
}

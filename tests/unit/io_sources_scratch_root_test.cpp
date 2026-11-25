#include "io_sources_fixture.h"

#include <meios/io/scratch_dir.h>
#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/resolved_asset.h>
#include <meios/io/directory_source.h>

#include <meios/expected.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <optional>
#include <filesystem>
#include <system_error>

namespace
{

// Windows models permissions loosely: filesystem::permissions cannot clear the group and
// others bits there, so status() reports them set whatever was asked for. Probing a
// directory of the test's own is what tells that host apart from one where the narrowing
// is real, so an unobservable narrowing is skipped loudly rather than passing silently.
bool narrowing_is_observable()
{
    const std::filesystem::path probe = fresh_dir();
    std::error_code ec;
    std::filesystem::create_directory(probe, ec);
    std::filesystem::permissions(probe, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, ec);
    const std::filesystem::perms mode = std::filesystem::status(probe).permissions();
    std::filesystem::remove_all(probe, ec);
    return (mode & std::filesystem::perms::others_read) == std::filesystem::perms::none;
}

}

TEST_CASE("a byte-backed source declines the empty relative for a package it carries", "[io][sources][scratch]")
{
    event_log events;
    meios::log_sink_f log{capture{events}};
    meios::log_sink &seam = log;
    meios::memory_source source{seam};
    source.add("pkg", "meshes/arm.dae", "collada-bytes");

    REQUIRE_FALSE(source.locate("pkg", "").has_value());
    REQUIRE(events.empty());
}

TEST_CASE("a stack reaches past a byte-backed layer to the layer that holds the package", "[io][sources][stack]")
{
    const std::filesystem::path root = seed_root();
    meios::log_sink log;
    meios::memory_source overlay{log};
    overlay.add("pkg", "meshes/override.stl", "override-bytes");
    meios::directory_source holder{root, log};
    meios::source_stack stacked{std::move(overlay), std::move(holder)};

    const std::optional<meios::resolved_asset> hit = stacked.locate("pkg", "", log);
    REQUIRE(hit.has_value());
    REQUIRE(std::filesystem::equivalent(hit->path(), root / "pkg"));
    REQUIRE(std::filesystem::exists(hit->path() / "meshes" / "x.stl"));

    std::filesystem::remove_all(root);
}

TEST_CASE("an entry offered under an empty relative is refused", "[io][sources][scratch]")
{
    event_log events;
    meios::log_sink_f log{capture{events}};
    meios::log_sink &seam = log;
    meios::memory_source source{seam};
    source.add("pkg", "", "bytes-that-name-no-file");

    REQUIRE(count_code(events, meios::level::error, meios::diagnostic_code::malformed_asset_uri) == 1);
}

TEST_CASE("a scratch root under a parent that cannot hold one is refused", "[io][sources][scratch]")
{
    const std::filesystem::path parent = fresh_dir() / "absent";
    REQUIRE_FALSE(meios::detail::create_scratch_root(parent).has_value());
}

TEST_CASE("a scratch root that failed permanently is not reported as a name collision", "[io][sources][scratch]")
{
    const std::filesystem::path parent                                            = fresh_dir() / "absent";
    const meios::expected<std::filesystem::path, meios::operation_failure> result = meios::detail::create_scratch_root(parent);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::create);
    REQUIRE(result.error().native.default_error_condition() != std::errc::file_exists);
}

TEST_CASE("the scratch root is reachable by its owner alone", "[io][sources][scratch]")
{
    if(!narrowing_is_observable())
    {
        WARN("this host does not express owner-only directory permissions; assertion skipped");
        return;
    }

    meios::log_sink log;
    meios::memory_source source{log};
    source.add("pkg", "meshes/arm.dae", "collada-bytes");

    const std::filesystem::path root  = locate_path(source, "meshes/arm.dae").parent_path().parent_path().parent_path();
    const std::filesystem::perms mode = std::filesystem::status(root).permissions();
    REQUIRE((mode & std::filesystem::perms::group_all) == std::filesystem::perms::none);
    REQUIRE((mode & std::filesystem::perms::others_all) == std::filesystem::perms::none);
}

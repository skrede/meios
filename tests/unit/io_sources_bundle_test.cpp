#include "io_sources_fixture.h"

#include <meios/io/bundle_source.h>
#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <filesystem>
#include <system_error>

static_assert(meios::package_source<meios::bundle_source>);
static_assert(meios::provides_path<meios::bundle_source>);

TEST_CASE("a relative bundle root resolves an in-root asset and still refuses an escape", "[io][sources][bundle]")
{
    const std::filesystem::path root = seed_root();
    std::error_code ec;
    const std::filesystem::path expected = std::filesystem::weakly_canonical(root / "pkg" / "meshes" / "x.stl", ec);
    event_log events;
    meios::log_sink_f log{capture{events}};
    meios::log_sink &seam = log;

    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(root.parent_path());
    meios::bundle_source source{root.filename(), seam};
    const std::optional<meios::resolved_asset> hit   = source.locate("pkg", "meshes/x.stl");
    const std::optional<std::filesystem::path> named = source.path_of("pkg", "meshes/x.stl");
    const bool escaped                               = source.locate("pkg", "../../etc/passwd").has_value();
    const std::size_t refusals                       = count_code(events, meios::level::error, meios::diagnostic_code::uncontained_asset);
    std::filesystem::current_path(saved);
    std::filesystem::remove_all(root);

    REQUIRE_FALSE(ec);
    REQUIRE(hit.has_value());
    REQUIRE(hit->path() == expected);
    REQUIRE(named == expected);
    REQUIRE_FALSE(escaped);
    REQUIRE(refusals == 1);
}

TEST_CASE("bundle_source resolves in-root and rejects escape like directory_source", "[io][sources][bundle]")
{
    std::filesystem::path root = seed_root();
    event_log events;
    meios::log_sink_f log{capture{events}};
    meios::log_sink &seam = log;
    meios::bundle_source source{root, seam};

    REQUIRE(source.capabilities().kind == meios::source_kind::bundle);
    REQUIRE(source.locate("pkg", "meshes/x.stl").has_value());
    REQUIRE_FALSE(source.locate("pkg", "../../etc/passwd").has_value());
    REQUIRE(count_code(events, meios::level::error, meios::diagnostic_code::uncontained_asset) == 1);

    std::filesystem::remove_all(root);
}

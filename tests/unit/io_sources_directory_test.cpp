#include "io_sources_fixture.h"

#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <fstream>
#include <optional>
#include <filesystem>
#include <system_error>

static_assert(meios::package_source<meios::directory_source>);
static_assert(meios::provides_path<meios::directory_source>);

TEST_CASE("a containment decision on a candidate that is not absolute is refused", "[io][sources][traversal]")
{
    REQUIRE_FALSE(meios::detail::contained_under("", "pkg/meshes/x.stl").has_value());
    REQUIRE_FALSE(meios::detail::contained_under("", "").has_value());
}

TEST_CASE("a containment decision on an absolute pair still answers", "[io][sources][traversal]")
{
    const std::filesystem::path root                = seed_root();
    const std::optional<std::filesystem::path> real = meios::detail::contained_under(root, root / "pkg" / "meshes" / "x.stl");
    REQUIRE(real.has_value());
    REQUIRE(real->filename() == "x.stl");
    std::filesystem::remove_all(root);
}

TEST_CASE("directory_source resolves an in-root file to a path", "[io][sources][dir]")
{
    std::filesystem::path root = seed_root();
    event_log events;
    meios::log_sink_f log{capture{events}};
    meios::log_sink &seam = log;
    meios::directory_source source{root, seam};

    const std::optional<meios::resolved_asset> hit = source.locate("pkg", "meshes/x.stl");
    REQUIRE(hit.has_value());
    REQUIRE(std::filesystem::equivalent(hit->path(), root / "pkg" / "meshes" / "x.stl"));
    REQUIRE(source.path_of("pkg", "meshes/x.stl").has_value());

    REQUIRE_FALSE(source.locate("pkg", "meshes/absent.stl").has_value());
    REQUIRE(events.empty());

    std::filesystem::remove_all(root);
}

TEST_CASE("a relative source root resolves an in-root asset and still refuses an escape", "[io][sources][dir]")
{
    const std::filesystem::path root = seed_root();
    std::error_code ec;
    const std::filesystem::path expected = std::filesystem::weakly_canonical(root / "pkg" / "meshes" / "x.stl", ec);
    event_log events;
    meios::log_sink_f log{capture{events}};
    meios::log_sink &seam = log;

    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(root.parent_path());
    meios::directory_source source{root.filename(), seam};
    const std::optional<meios::resolved_asset> hit   = source.locate("pkg", "meshes/x.stl");
    const meios::source_lookup_result typed          = source.try_locate("pkg", "meshes/x.stl");
    const std::optional<std::filesystem::path> named = source.path_of("pkg", "meshes/x.stl");
    const bool escaped                               = source.locate("pkg", "../../etc/passwd").has_value();
    const std::size_t refusals                       = count_code(events, meios::level::error, meios::diagnostic_code::uncontained_asset);
    std::filesystem::current_path(saved);
    std::filesystem::remove_all(root);

    REQUIRE_FALSE(ec);
    REQUIRE(hit.has_value());
    REQUIRE(hit->path() == expected);
    REQUIRE(hit->source_root().has_value());
    REQUIRE(hit->source_root()->is_absolute());
    REQUIRE(typed.has_value());
    REQUIRE(typed->has_value());
    REQUIRE((*typed)->path() == expected);
    REQUIRE(named == expected);
    REQUIRE_FALSE(escaped);
    REQUIRE(refusals == 1);
}

TEST_CASE("directory_source rejects a traversal escape with a loud diagnostic", "[io][sources][traversal]")
{
    std::filesystem::path root = seed_root();
    event_log events;
    meios::log_sink_f log{capture{events}};
    meios::log_sink &seam = log;
    meios::directory_source source{root, seam};

    REQUIRE_FALSE(source.locate("pkg", "../../etc/passwd").has_value());
    REQUIRE_FALSE(source.locate("pkg", "/etc/passwd").has_value());
    REQUIRE_FALSE(source.path_of("pkg", "../../etc/passwd").has_value());
    REQUIRE(count_code(events, meios::level::error, meios::diagnostic_code::uncontained_asset) == 3);

    std::filesystem::remove_all(root);
}

TEST_CASE("directory_source rejects an escaping in-root symlink and never reads outside the root", "[io][sources][symlink]")
{
    std::filesystem::path parent  = fresh_dir();
    std::filesystem::path root    = parent / "ws";
    std::filesystem::path outside = parent / "secret";
    std::filesystem::create_directories(root);
    std::filesystem::create_directories(outside);
    std::ofstream(outside / "passwd") << "secret-bytes";

    std::error_code ec;
    std::filesystem::create_directory_symlink(outside, root / "evil", ec);
    if(ec)
    {
        std::filesystem::remove_all(parent);
        SUCCEED("platform cannot create a directory symlink; skipping");
        return;
    }

    event_log events;
    meios::log_sink_f log{capture{events}};
    meios::log_sink &seam = log;
    meios::directory_source source{root, seam};

    // The raw --package-path resolver has no package registration, so an in-root
    // symlink whose real target escapes the root is rejected outright; the file
    // behind evil must never be handed back. Symlink-install lives at the ros layer.
    REQUIRE_FALSE(source.locate("evil", "passwd").has_value());
    REQUIRE_FALSE(source.path_of("evil", "passwd").has_value());
    REQUIRE(count_code(events, meios::level::error, meios::diagnostic_code::uncontained_asset) == 2);

    std::filesystem::remove_all(parent);
}

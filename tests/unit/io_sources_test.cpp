#include <meios/io/scratch_dir.h>
#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/bundle_source.h>
#include <meios/io/directory_source.h>
#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <random>
#include <cstddef>
#include <utility>
#include <fstream>
#include <iterator>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

static_assert(meios::package_source<meios::memory_source>);
static_assert(!meios::provides_path<meios::memory_source>);
static_assert(meios::package_source<meios::directory_source>);
static_assert(meios::provides_path<meios::directory_source>);
static_assert(meios::package_source<meios::bundle_source>);
static_assert(meios::provides_path<meios::bundle_source>);

namespace
{

struct event
{
    meios::level           lvl;
    meios::diagnostic_code code;
};

using event_log = std::vector<event>;

struct capture
{
    event_log &events;

    void operator()(meios::level lvl, const std::string &)
    {
        events.push_back({ lvl, meios::diagnostic_code::unspecified });
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &)
    {
        events.push_back({ lvl, meios::diagnostic_code::unspecified });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &)
    {
        events.push_back({ lvl, code });
    }
};

std::size_t count_code(const event_log &events, meios::level lvl, meios::diagnostic_code code)
{
    std::size_t total = 0;
    for(const event &recorded : events)
        if(recorded.lvl == lvl && recorded.code == code)
            ++total;
    return total;
}

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::filesystem::path locate_path(meios::memory_source &source, const char *relative)
{
    const std::optional<meios::resolved_asset> hit = source.locate("pkg", relative);
    REQUIRE(hit.has_value());
    return hit->path();
}

std::filesystem::path fresh_dir()
{
    std::random_device device;
    for(;;)
    {
        std::filesystem::path candidate = std::filesystem::temp_directory_path()
                                        / ("meios-io-" + std::to_string(device()));
        if(!std::filesystem::exists(candidate))
            return candidate;
    }
}

std::filesystem::path seed_root()
{
    std::filesystem::path root = fresh_dir();
    std::filesystem::create_directories(root / "pkg" / "meshes");
    std::ofstream(root / "pkg" / "meshes" / "x.stl") << "mesh-bytes";
    return root;
}

// Windows models permissions loosely: filesystem::permissions cannot clear the group and
// others bits there, so status() reports them set whatever was asked for. Probing a
// directory of the test's own is what tells that host apart from one where the narrowing
// is real, so an unobservable narrowing is skipped loudly rather than passing silently.
bool narrowing_is_observable()
{
    const std::filesystem::path probe = fresh_dir();
    std::error_code ec;
    std::filesystem::create_directory(probe, ec);
    std::filesystem::permissions(probe, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
    const std::filesystem::perms mode = std::filesystem::status(probe).permissions();
    std::filesystem::remove_all(probe, ec);
    return (mode & std::filesystem::perms::others_read) == std::filesystem::perms::none;
}

}

TEST_CASE("memory_source resolves a stored key to a real path and misses to nullopt",
          "[io][sources][memory]")
{
    meios::log_sink log;
    meios::memory_source source{ log };
    source.add("pkg", "a/x.stl", "solid-bytes");

    const std::filesystem::path located = locate_path(source, "a/x.stl");
    REQUIRE(std::filesystem::exists(located));
    REQUIRE(read_file(located) == "solid-bytes");

    REQUIRE_FALSE(source.locate("pkg", "a/missing.stl").has_value());
}

TEST_CASE("a scratch entry keeps the authored extension and mirrors the requested path",
          "[io][sources][scratch]")
{
    meios::log_sink log;
    meios::memory_source source{ log };
    source.add("pkg", "meshes/arm.dae", "collada-bytes");

    const std::filesystem::path located = locate_path(source, "meshes/arm.dae");
    REQUIRE(located.extension() == ".dae");
    REQUIRE(located.parent_path().filename() == "meshes");
}

TEST_CASE("a sibling entry resolves by a plain relative join from a located asset",
          "[io][sources][scratch]")
{
    meios::log_sink log;
    meios::memory_source source{ log };
    source.add("pkg", "meshes/arm.dae", "collada-bytes");
    source.add("pkg", "meshes/arm.mtl", "material-bytes");

    const std::filesystem::path model = locate_path(source, "meshes/arm.dae");
    const std::filesystem::path sibling = locate_path(source, "meshes/arm.mtl");
    REQUIRE(sibling == model.parent_path() / "arm.mtl");
}

TEST_CASE("an entry no locate asked for is never written", "[io][sources][scratch]")
{
    meios::log_sink log;
    meios::memory_source source{ log };
    source.add("pkg", "meshes/arm.dae", "collada-bytes");
    source.add("pkg", "meshes/leg.dae", "collada-bytes");

    const std::filesystem::path located = locate_path(source, "meshes/arm.dae");
    REQUIRE(std::filesystem::exists(located));
    REQUIRE_FALSE(std::filesystem::exists(located.parent_path() / "leg.dae"));
}

TEST_CASE("locating the same key twice hands back the same path", "[io][sources][scratch]")
{
    meios::log_sink log;
    meios::memory_source source{ log };
    source.add("pkg", "meshes/arm.dae", "collada-bytes");

    REQUIRE(locate_path(source, "meshes/arm.dae") == locate_path(source, "meshes/arm.dae"));
}

TEST_CASE("the scratch tree is removed when the source that owns it dies",
          "[io][sources][scratch]")
{
    meios::log_sink log;
    std::filesystem::path recorded;
    {
        meios::memory_source source{ log };
        source.add("pkg", "meshes/arm.dae", "collada-bytes");
        recorded = locate_path(source, "meshes/arm.dae");
        REQUIRE(std::filesystem::exists(recorded));
    }
    REQUIRE_FALSE(std::filesystem::exists(recorded));
}

TEST_CASE("a byte-backed source declines the empty relative for a package it carries",
          "[io][sources][scratch]")
{
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;
    meios::memory_source source{ seam };
    source.add("pkg", "meshes/arm.dae", "collada-bytes");

    REQUIRE_FALSE(source.locate("pkg", "").has_value());
    REQUIRE(events.empty());
}

TEST_CASE("a stack reaches past a byte-backed layer to the layer that holds the package",
          "[io][sources][stack]")
{
    const std::filesystem::path root = seed_root();
    meios::log_sink log;
    meios::memory_source overlay{ log };
    overlay.add("pkg", "meshes/override.stl", "override-bytes");
    meios::directory_source holder{ root, log };
    meios::source_stack stacked{ std::move(overlay), std::move(holder) };

    const std::optional<meios::resolved_asset> hit = stacked.locate("pkg", "", log);
    REQUIRE(hit.has_value());
    REQUIRE(std::filesystem::equivalent(hit->path(), root / "pkg"));
    REQUIRE(std::filesystem::exists(hit->path() / "meshes" / "x.stl"));

    std::filesystem::remove_all(root);
}

TEST_CASE("an entry offered under an empty relative is refused", "[io][sources][scratch]")
{
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;
    meios::memory_source source{ seam };
    source.add("pkg", "", "bytes-that-name-no-file");

    REQUIRE(count_code(events, meios::level::error,
                       meios::diagnostic_code::malformed_asset_uri)
            == 1);
}

TEST_CASE("a scratch root under a parent that cannot hold one is refused",
          "[io][sources][scratch]")
{
    const std::filesystem::path parent = fresh_dir() / "absent";
    std::error_code ec;
    REQUIRE_FALSE(meios::detail::create_scratch_root(parent, ec).has_value());
    REQUIRE(static_cast<bool>(ec));
}

TEST_CASE("a scratch root that failed permanently is not reported as a name collision",
          "[io][sources][scratch]")
{
    const std::filesystem::path parent = fresh_dir() / "absent";
    std::error_code ec;
    REQUIRE_FALSE(meios::detail::create_scratch_root(parent, ec).has_value());
    REQUIRE(ec.default_error_condition() != std::errc::file_exists);
}

TEST_CASE("the scratch root is reachable by its owner alone", "[io][sources][scratch]")
{
    if(!narrowing_is_observable())
    {
        WARN("this host does not express owner-only directory permissions; assertion skipped");
        return;
    }

    meios::log_sink log;
    meios::memory_source source{ log };
    source.add("pkg", "meshes/arm.dae", "collada-bytes");

    const std::filesystem::path root =
        locate_path(source, "meshes/arm.dae").parent_path().parent_path().parent_path();
    const std::filesystem::perms mode = std::filesystem::status(root).permissions();
    REQUIRE((mode & std::filesystem::perms::group_all) == std::filesystem::perms::none);
    REQUIRE((mode & std::filesystem::perms::others_all) == std::filesystem::perms::none);
}

TEST_CASE("a containment decision on a candidate that is not absolute is refused",
          "[io][sources][traversal]")
{
    REQUIRE_FALSE(meios::detail::contained_under("", "pkg/meshes/x.stl").has_value());
    REQUIRE_FALSE(meios::detail::contained_under("", "").has_value());
}

TEST_CASE("a containment decision on an absolute pair still answers", "[io][sources][traversal]")
{
    const std::filesystem::path root = seed_root();
    const std::optional<std::filesystem::path> real =
        meios::detail::contained_under(root, root / "pkg" / "meshes" / "x.stl");
    REQUIRE(real.has_value());
    REQUIRE(real->filename() == "x.stl");
    std::filesystem::remove_all(root);
}

TEST_CASE("directory_source resolves an in-root file to a path", "[io][sources][dir]")
{
    std::filesystem::path root = seed_root();
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;
    meios::directory_source source{ root, seam };

    const std::optional<meios::resolved_asset> hit = source.locate("pkg", "meshes/x.stl");
    REQUIRE(hit.has_value());
    REQUIRE(std::filesystem::equivalent(hit->path(), root / "pkg" / "meshes" / "x.stl"));
    REQUIRE(source.path_of("pkg", "meshes/x.stl").has_value());

    REQUIRE_FALSE(source.locate("pkg", "meshes/absent.stl").has_value());
    REQUIRE(events.empty());

    std::filesystem::remove_all(root);
}

TEST_CASE("directory_source rejects a traversal escape with a loud diagnostic",
          "[io][sources][traversal]")
{
    std::filesystem::path root = seed_root();
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;
    meios::directory_source source{ root, seam };

    REQUIRE_FALSE(source.locate("pkg", "../../etc/passwd").has_value());
    REQUIRE_FALSE(source.locate("pkg", "/etc/passwd").has_value());
    REQUIRE_FALSE(source.path_of("pkg", "../../etc/passwd").has_value());
    REQUIRE(count_code(events, meios::level::error, meios::diagnostic_code::uncontained_asset) == 3);

    std::filesystem::remove_all(root);
}

TEST_CASE("directory_source rejects an escaping in-root symlink and never reads outside the root",
          "[io][sources][symlink]")
{
    std::filesystem::path parent = fresh_dir();
    std::filesystem::path root = parent / "ws";
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
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;
    meios::directory_source source{ root, seam };

    // The raw --package-path resolver has no package registration, so an in-root
    // symlink whose real target escapes the root is rejected outright; the file
    // behind evil must never be handed back. Symlink-install lives at the ros layer.
    REQUIRE_FALSE(source.locate("evil", "passwd").has_value());
    REQUIRE_FALSE(source.path_of("evil", "passwd").has_value());
    REQUIRE(count_code(events, meios::level::error, meios::diagnostic_code::uncontained_asset) == 2);

    std::filesystem::remove_all(parent);
}

TEST_CASE("bundle_source resolves in-root and rejects escape like directory_source",
          "[io][sources][bundle]")
{
    std::filesystem::path root = seed_root();
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;
    meios::bundle_source source{ root, seam };

    REQUIRE(source.capabilities().kind == meios::source_kind::bundle);
    REQUIRE(source.locate("pkg", "meshes/x.stl").has_value());
    REQUIRE_FALSE(source.locate("pkg", "../../etc/passwd").has_value());
    REQUIRE(count_code(events, meios::level::error, meios::diagnostic_code::uncontained_asset) == 1);

    std::filesystem::remove_all(root);
}

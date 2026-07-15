#include <meios/io/memory_source.h>
#include <meios/io/materialize.h>
#include <meios/io/bundle_source.h>
#include <meios/io/directory_source.h>
#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <array>
#include <string>
#include <vector>
#include <random>
#include <utility>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
#include <filesystem>
#include <string_view>

static_assert(meios::package_source<meios::memory_source>);
static_assert(!meios::provides_path<meios::memory_source>);
static_assert(meios::package_source<meios::directory_source>);
static_assert(meios::provides_path<meios::directory_source>);
static_assert(meios::package_source<meios::bundle_source>);
static_assert(meios::provides_path<meios::bundle_source>);

namespace
{

using event_log = std::vector<std::pair<meios::level, std::string>>;

struct capture
{
    event_log &events;

    void operator()(meios::level lvl, const std::string &message)
    {
        events.push_back({ lvl, message });
    }

    void operator()(meios::level, const meios::source_location &, const std::string &) {}
};

std::size_t count_level(const event_log &events, meios::level lvl)
{
    std::size_t total = 0;
    for(const std::pair<meios::level, std::string> &event : events)
        if(event.first == lvl)
            ++total;
    return total;
}

std::string read_all(meios::resolved_asset &asset)
{
    std::array<std::byte, 8> buffer{};
    std::string out;
    for(std::size_t n = asset.bytes().read(buffer); n != 0; n = asset.bytes().read(buffer))
        out.append(reinterpret_cast<const char *>(buffer.data()), n);
    return out;
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

}

TEST_CASE("memory_source resolves stored keys and misses to nullopt", "[io][sources][memory]")
{
    meios::memory_source source;
    source.add("pkg", "a/x.stl", "solid-bytes");

    std::optional<meios::resolved_asset> hit = source.locate("pkg", "a/x.stl");
    REQUIRE(hit.has_value());
    REQUIRE(hit->holds_bytes());
    REQUIRE(read_all(*hit) == "solid-bytes");

    REQUIRE_FALSE(source.locate("pkg", "a/missing.stl").has_value());
    REQUIRE(source.capabilities().path_backed == false);
}

TEST_CASE("directory_source resolves an in-root file to a path", "[io][sources][dir]")
{
    std::filesystem::path root = seed_root();
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;
    meios::directory_source source{ root, seam };

    std::optional<meios::resolved_asset> hit = source.locate("pkg", "meshes/x.stl");
    REQUIRE(hit.has_value());
    REQUIRE(hit->holds_path());
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
    REQUIRE(count_level(events, meios::level::error) == 3);

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
    REQUIRE(count_level(events, meios::level::error) == 2);

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
    REQUIRE(count_level(events, meios::level::error) == 1);

    std::filesystem::remove_all(root);
}

TEST_CASE("materialize writes bytes to a temp file, logs once, and unlinks on destruction",
          "[io][sources][materialize]")
{
    meios::memory_source source;
    source.add("pkg", "a/x.stl", "payload-bytes");
    std::optional<meios::resolved_asset> bytes = source.locate("pkg", "a/x.stl");
    REQUIRE(bytes.has_value());

    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;

    std::filesystem::path recorded;
    {
        meios::resolved_asset materialized = meios::materialize(std::move(*bytes), seam);
        REQUIRE(materialized.holds_path());
        recorded = materialized.path();
        REQUIRE(std::filesystem::exists(recorded));

        std::ifstream in(recorded, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        REQUIRE(content == "payload-bytes");
        REQUIRE(count_level(events, meios::level::info) == 1);
    }
    REQUIRE_FALSE(std::filesystem::exists(recorded));
}

TEST_CASE("materialize passes a path-backed asset through untouched",
          "[io][sources][materialize]")
{
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;

    meios::resolved_asset original{ std::filesystem::path{ "already/on/disk.stl" } };
    meios::resolved_asset result = meios::materialize(std::move(original), seam);

    REQUIRE(result.holds_path());
    REQUIRE(result.path() == std::filesystem::path{ "already/on/disk.stl" });
    REQUIRE(events.empty());
}

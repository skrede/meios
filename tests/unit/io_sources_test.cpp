#include "io_sources_fixture.h"

#include <meios/io/memory_source.h>
#include <meios/io/package_source.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <iterator>
#include <filesystem>

static_assert(meios::package_source<meios::memory_source>);
static_assert(!meios::provides_path<meios::memory_source>);

namespace
{

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}

TEST_CASE("memory_source resolves a stored key to a real path and misses to nullopt", "[io][sources][memory]")
{
    meios::log_sink log;
    meios::memory_source source{log};
    source.add("pkg", "a/x.stl", "solid-bytes");

    const std::filesystem::path located = locate_path(source, "a/x.stl");
    REQUIRE(std::filesystem::exists(located));
    REQUIRE(read_file(located) == "solid-bytes");

    REQUIRE_FALSE(source.locate("pkg", "a/missing.stl").has_value());
}

TEST_CASE("a scratch entry keeps the authored extension and mirrors the requested path", "[io][sources][scratch]")
{
    meios::log_sink log;
    meios::memory_source source{log};
    source.add("pkg", "meshes/arm.dae", "collada-bytes");

    const std::filesystem::path located = locate_path(source, "meshes/arm.dae");
    REQUIRE(located.extension() == ".dae");
    REQUIRE(located.parent_path().filename() == "meshes");
}

TEST_CASE("a sibling entry resolves by a plain relative join from a located asset", "[io][sources][scratch]")
{
    meios::log_sink log;
    meios::memory_source source{log};
    source.add("pkg", "meshes/arm.dae", "collada-bytes");
    source.add("pkg", "meshes/arm.mtl", "material-bytes");

    const std::filesystem::path model   = locate_path(source, "meshes/arm.dae");
    const std::filesystem::path sibling = locate_path(source, "meshes/arm.mtl");
    REQUIRE(sibling == model.parent_path() / "arm.mtl");
}

TEST_CASE("an entry no locate asked for is never written", "[io][sources][scratch]")
{
    meios::log_sink log;
    meios::memory_source source{log};
    source.add("pkg", "meshes/arm.dae", "collada-bytes");
    source.add("pkg", "meshes/leg.dae", "collada-bytes");

    const std::filesystem::path located = locate_path(source, "meshes/arm.dae");
    REQUIRE(std::filesystem::exists(located));
    REQUIRE_FALSE(std::filesystem::exists(located.parent_path() / "leg.dae"));
}

TEST_CASE("a package nothing has been located under has no directory at all", "[io][sources][scratch]")
{
    meios::log_sink log;
    meios::memory_source source{log};
    source.add("pkg", "meshes/arm.dae", "collada-bytes");
    source.add("other", "meshes/leg.dae", "collada-bytes");

    const std::filesystem::path root = locate_path(source, "meshes/arm.dae").parent_path().parent_path().parent_path();
    REQUIRE(std::filesystem::exists(root / "pkg"));
    REQUIRE_FALSE(std::filesystem::exists(root / "other"));
}

TEST_CASE("locating the same key twice hands back the same path", "[io][sources][scratch]")
{
    meios::log_sink log;
    meios::memory_source source{log};
    source.add("pkg", "meshes/arm.dae", "collada-bytes");

    REQUIRE(locate_path(source, "meshes/arm.dae") == locate_path(source, "meshes/arm.dae"));
}

TEST_CASE("the scratch tree is removed when the source that owns it dies", "[io][sources][scratch]")
{
    meios::log_sink log;
    std::filesystem::path recorded;
    {
        meios::memory_source source{log};
        source.add("pkg", "meshes/arm.dae", "collada-bytes");
        recorded = locate_path(source, "meshes/arm.dae");
        REQUIRE(std::filesystem::exists(recorded));
    }
    REQUIRE_FALSE(std::filesystem::exists(recorded));
}

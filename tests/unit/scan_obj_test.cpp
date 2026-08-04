#include "asset_read_probe.h"

#include <meios/scan/obj_scanner.h>

#include <meios/bundle.h>

#include <meios/io/memory_source.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>

namespace
{

meios::resolved_asset fixture(const char *rel)
{
    return meios::resolved_asset{ std::filesystem::path{ MEIOS_ASSET_FIXTURE_DIR } / rel };
}

// A source owns the scratch its assets live in, so the two are held together: the path
// handed out here stays readable for exactly as long as this fixture does.
class inline_asset
{
public:
    explicit inline_asset(std::string data) : m_log(), m_source(m_log), m_located()
    {
        m_source.add("pkg", "inline", std::move(data));
        m_located = m_source.locate("pkg", "inline");
    }

    const meios::resolved_asset &asset() const { return m_located.value(); }

private:
    meios::log_sink m_log;
    meios::memory_source m_source;
    std::optional<meios::resolved_asset> m_located;
};

}

static_assert(meios::asset_scanner<meios::obj_scanner>);

TEST_CASE("an obj mtllib statement yields its library ref", "[scan][obj]")
{
    meios::obj_scanner scanner;
    meios::log_sink silent;
    meios::resolved_asset asset = fixture("obj/cube.obj");

    REQUIRE(scanner.scan(asset, silent) == std::vector<std::string>{ "cube.mtl" });
}

TEST_CASE("an mtl file yields each map_* texture and no option tokens", "[scan][obj]")
{
    meios::obj_scanner scanner;
    meios::log_sink silent;
    meios::resolved_asset asset = fixture("obj/cube.mtl");

    REQUIRE(scanner.scan(asset, silent) == std::vector<std::string>{ "wood.png", "rough.png" });
}

TEST_CASE("leading whitespace, CR, comments and map options are tolerated", "[scan][obj]")
{
    meios::obj_scanner scanner;
    meios::log_sink silent;
    const inline_asset held{ "  mtllib a.mtl b.mtl\r\n# a comment\nmap_Kd -bm 0.5 tex.png\n" };

    REQUIRE(scanner.scan(held.asset(), silent)
            == std::vector<std::string>{ "a.mtl", "b.mtl", "tex.png" });
}

TEST_CASE("an asset written into a source's scratch is scanned via the shared read helper",
          "[scan][obj]")
{
    meios::obj_scanner scanner;
    meios::log_sink silent;
    const inline_asset held{ "mtllib inline.mtl\n" };

    REQUIRE(scanner.scan(held.asset(), silent) == std::vector<std::string>{ "inline.mtl" });
}

TEST_CASE("a failed read is one coded record and stops before the parser", "[scan][obj]")
{
    meios::obj_scanner scanner;

    asset_probe::drive_refusal(scanner, "obj");
}

TEST_CASE("an empty regular file still reaches the parser", "[scan][obj]")
{
    meios::obj_scanner scanner;

    asset_probe::drive_empty(scanner, "obj", false);
}

TEST_CASE("registration wires both the obj and mtl extensions", "[scan][obj]")
{
    meios::scanner_registry registry;
    meios::log_sink silent;
    meios::register_obj_scanner(registry);
    meios::resolved_asset mtl = fixture("obj/cube.mtl");

    REQUIRE(registry.contains("obj"));
    REQUIRE(registry.contains("mtl"));
    REQUIRE(registry.scan("mtl", mtl, silent)
            == std::vector<std::string>{ "wood.png", "rough.png" });
}

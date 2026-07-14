#include <meios/scan/obj_scanner.h>

#include <meios/bundle.h>

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>
#include <memory>
#include <cstddef>
#include <cstring>
#include <utility>
#include <algorithm>
#include <filesystem>

namespace
{

meios::resolved_asset fixture(const char *rel)
{
    return meios::resolved_asset{ std::filesystem::path{ MEIOS_ASSET_FIXTURE_DIR } / rel };
}

struct string_puller final : meios::byte_reader::puller
{
    explicit string_puller(std::string data) : m_data(std::move(data)) {}

    std::size_t read(std::span<std::byte> out) override
    {
        const std::size_t n = std::min(out.size(), m_data.size() - m_pos);
        std::memcpy(out.data(), m_data.data() + m_pos, n);
        m_pos += n;
        return n;
    }

    std::string m_data;
    std::size_t m_pos = 0;
};

meios::resolved_asset bytes_asset(std::string data)
{
    return meios::resolved_asset{ meios::byte_reader{ std::make_unique<string_puller>(std::move(data)) } };
}

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
    meios::resolved_asset asset =
        bytes_asset("  mtllib a.mtl b.mtl\r\n# a comment\nmap_Kd -bm 0.5 tex.png\n");

    REQUIRE(scanner.scan(asset, silent)
            == std::vector<std::string>{ "a.mtl", "b.mtl", "tex.png" });
}

TEST_CASE("a bytes-backed asset is scanned via the shared read helper", "[scan][obj]")
{
    meios::obj_scanner scanner;
    meios::log_sink silent;
    meios::resolved_asset asset = bytes_asset("mtllib inline.mtl\n");

    REQUIRE(scanner.scan(asset, silent) == std::vector<std::string>{ "inline.mtl" });
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

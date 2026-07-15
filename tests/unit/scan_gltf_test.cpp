#include <meios/scan/gltf_scanner.h>

#include <meios/bundle.h>

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>
#include <memory>
#include <cstddef>
#include <cstdint>
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

struct counting_sink final : meios::log_sink
{
    void log(meios::level lvl, const std::string &) override
    {
        if(lvl == meios::level::error)
            ++errors;
    }

    using meios::log_sink::log;

    int errors = 0;
};

void put_u32(std::string &out, std::uint32_t value)
{
    out.push_back(static_cast<char>(value & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
    out.push_back(static_cast<char>((value >> 16) & 0xFF));
    out.push_back(static_cast<char>((value >> 24) & 0xFF));
}

std::string make_glb(std::string json, std::size_t bin_bytes)
{
    while(json.size() % 4 != 0)
        json.push_back(' ');
    std::string bin(bin_bytes, '\0');
    while(bin.size() % 4 != 0)
        bin.push_back('\0');
    std::string out;
    put_u32(out, 0x46546C67);
    put_u32(out, 2);
    put_u32(out, static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + bin.size()));
    put_u32(out, static_cast<std::uint32_t>(json.size()));
    put_u32(out, 0x4E4F534A);
    out += json;
    put_u32(out, static_cast<std::uint32_t>(bin.size()));
    put_u32(out, 0x004E4942);
    out += bin;
    return out;
}

}

static_assert(meios::asset_scanner<meios::gltf_scanner>);

TEST_CASE("a gltf reports external image and buffer uris but not a data: uri", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    meios::log_sink silent;
    meios::resolved_asset asset = fixture("gltf/box.gltf");

    REQUIRE(scanner.scan(asset, silent)
            == std::vector<std::string>{ "textures/albedo.png", "box.bin" });
}

TEST_CASE("a fully embedded glb reports zero external assets", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    meios::log_sink silent;
    meios::resolved_asset asset = bytes_asset(make_glb(
        R"({"images":[{"uri":"data:image/png;base64,AAAA"}],"buffers":[{"byteLength":4}]})", 4));

    REQUIRE(scanner.scan(asset, silent).empty());
}

TEST_CASE("a glb with an external buffer uri reports it", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    meios::log_sink silent;
    meios::resolved_asset asset = bytes_asset(make_glb(
        R"({"buffers":[{"uri":"external.bin","byteLength":4}]})", 4));

    REQUIRE(scanner.scan(asset, silent) == std::vector<std::string>{ "external.bin" });
}

TEST_CASE("a bad-magic glb yields a diagnostic and an empty list", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    counting_sink log;
    std::string glb = make_glb(R"({"buffers":[]})", 4);
    glb[0] = 'x';
    meios::resolved_asset asset = bytes_asset(std::move(glb));

    REQUIRE(scanner.scan(asset, log).empty());
}

TEST_CASE("a wrong-version glb yields a diagnostic and an empty list", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    counting_sink log;
    std::string glb = make_glb(R"({"buffers":[]})", 4);
    glb[4] = 3;
    meios::resolved_asset asset = bytes_asset(std::move(glb));

    REQUIRE(scanner.scan(asset, log).empty());
    REQUIRE(log.errors > 0);
}

TEST_CASE("an oversized declared length yields a diagnostic and an empty list", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    counting_sink log;
    std::string glb = make_glb(R"({"buffers":[]})", 4);
    glb[8] = static_cast<char>(0xFF);
    glb[9] = static_cast<char>(0xFF);
    meios::resolved_asset asset = bytes_asset(std::move(glb));

    REQUIRE(scanner.scan(asset, log).empty());
    REQUIRE(log.errors > 0);
}

TEST_CASE("a declared length shorter than the actual byte count yields a diagnostic and an empty list", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    counting_sink log;
    std::string glb = make_glb(R"({"buffers":[]})", 4);
    glb.append(4, '\0');
    meios::resolved_asset asset = bytes_asset(std::move(glb));

    REQUIRE(scanner.scan(asset, log).empty());
    REQUIRE(log.errors > 0);
}

TEST_CASE("registration wires the gltf and glb extensions", "[scan][gltf]")
{
    meios::scanner_registry registry;
    meios::log_sink silent;
    meios::register_gltf_scanner(registry);
    meios::resolved_asset asset = fixture("gltf/box.gltf");

    REQUIRE(registry.contains("gltf"));
    REQUIRE(registry.contains("glb"));
    REQUIRE(registry.scan("gltf", asset, silent)
            == std::vector<std::string>{ "textures/albedo.png", "box.bin" });
}

#include <meios/scan/gltf_scanner.h>

#include <meios/bundle.h>

#include <meios/io/memory_source.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
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
    const inline_asset held{ make_glb(
        R"({"images":[{"uri":"data:image/png;base64,AAAA"}],"buffers":[{"byteLength":4}]})", 4) };

    REQUIRE(scanner.scan(held.asset(), silent).empty());
}

TEST_CASE("a glb with an external buffer uri reports it", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    meios::log_sink silent;
    const inline_asset held{ make_glb(R"({"buffers":[{"uri":"external.bin","byteLength":4}]})", 4) };

    REQUIRE(scanner.scan(held.asset(), silent) == std::vector<std::string>{ "external.bin" });
}

TEST_CASE("a bad-magic glb yields a diagnostic and an empty list", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    counting_sink log;
    std::string glb = make_glb(R"({"buffers":[]})", 4);
    glb[0] = 'x';
    const inline_asset held{ std::move(glb) };

    REQUIRE(scanner.scan(held.asset(), log).empty());
}

TEST_CASE("a wrong-version glb yields a diagnostic and an empty list", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    counting_sink log;
    std::string glb = make_glb(R"({"buffers":[]})", 4);
    glb[4] = 3;
    const inline_asset held{ std::move(glb) };

    REQUIRE(scanner.scan(held.asset(), log).empty());
    REQUIRE(log.errors > 0);
}

TEST_CASE("an oversized declared length yields a diagnostic and an empty list", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    counting_sink log;
    std::string glb = make_glb(R"({"buffers":[]})", 4);
    glb[8] = static_cast<char>(0xFF);
    glb[9] = static_cast<char>(0xFF);
    const inline_asset held{ std::move(glb) };

    REQUIRE(scanner.scan(held.asset(), log).empty());
    REQUIRE(log.errors > 0);
}

TEST_CASE("a declared length shorter than the actual byte count yields a diagnostic and an empty list", "[scan][gltf]")
{
    meios::gltf_scanner scanner;
    counting_sink log;
    std::string glb = make_glb(R"({"buffers":[]})", 4);
    glb.append(4, '\0');
    const inline_asset held{ std::move(glb) };

    REQUIRE(scanner.scan(held.asset(), log).empty());
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

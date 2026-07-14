#include <meios/scan/collada_scanner.h>

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

}

static_assert(meios::asset_scanner<meios::collada_scanner>);

TEST_CASE("a dae yields both the 1.4 text and 1.5 ref init_from paths", "[scan][collada]")
{
    meios::collada_scanner scanner;
    meios::log_sink silent;
    meios::resolved_asset asset = fixture("dae/box.dae");

    REQUIRE(scanner.scan(asset, silent)
            == std::vector<std::string>{ "textures/wood.png", "textures/rough.png" });
}

TEST_CASE("a file:// prefix is stripped and a percent escape is decoded", "[scan][collada]")
{
    meios::collada_scanner scanner;
    meios::log_sink silent;
    meios::resolved_asset asset = bytes_asset(
        "<COLLADA><library_images><image>"
        "<init_from>file://textures/a%20b.png</init_from>"
        "</image></library_images></COLLADA>");

    REQUIRE(scanner.scan(asset, silent) == std::vector<std::string>{ "textures/a b.png" });
}

TEST_CASE("a malformed dae yields a diagnostic and an empty list", "[scan][collada]")
{
    meios::collada_scanner scanner;
    counting_sink log;
    meios::resolved_asset asset = bytes_asset("<COLLADA><library_images><image>");

    REQUIRE(scanner.scan(asset, log).empty());
    REQUIRE(log.errors > 0);
}

TEST_CASE("registration wires the dae extension", "[scan][collada]")
{
    meios::scanner_registry registry;
    meios::log_sink silent;
    meios::register_collada_scanner(registry);
    meios::resolved_asset asset = fixture("dae/box.dae");

    REQUIRE(registry.contains("dae"));
    REQUIRE(registry.scan("dae", asset, silent)
            == std::vector<std::string>{ "textures/wood.png", "textures/rough.png" });
}

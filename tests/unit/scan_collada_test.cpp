#include <meios/scan/collada_scanner.h>

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
    const inline_asset held{
        "<COLLADA><library_images><image>"
        "<init_from>file://textures/a%20b.png</init_from>"
        "</image></library_images></COLLADA>"
    };

    REQUIRE(scanner.scan(held.asset(), silent) == std::vector<std::string>{ "textures/a b.png" });
}

TEST_CASE("a malformed dae yields a diagnostic and an empty list", "[scan][collada]")
{
    meios::collada_scanner scanner;
    counting_sink log;
    const inline_asset held{ "<COLLADA><library_images><image>" };

    REQUIRE(scanner.scan(held.asset(), log).empty());
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

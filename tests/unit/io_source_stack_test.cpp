#include <meios/io/source_stack.h>
#include <meios/io/source_handle.h>
#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

struct info_counter
{
    int &count;

    void operator()(meios::level lvl, const std::string &)
    {
        if(lvl == meios::level::info)
            ++count;
    }

    void operator()(meios::level, const meios::source_location &, const std::string &) {}
};

struct fixture_source
{
    std::string tag;
    std::set<std::pair<std::string, std::string>> answers;

    meios::capability_descriptor capabilities() const
    {
        return { meios::source_kind::memory, false };
    }

    std::optional<meios::resolved_asset> locate(std::string_view pkg, std::string_view rel)
    {
        if(answers.count({ std::string(pkg), std::string(rel) }) == 0)
            return std::nullopt;
        return meios::resolved_asset{ std::filesystem::path{ tag } };
    }
};

struct path_fixture
{
    meios::capability_descriptor capabilities() const
    {
        return { meios::source_kind::directory, false };
    }

    std::optional<meios::resolved_asset> locate(std::string_view, std::string_view)
    {
        return std::nullopt;
    }

    std::optional<std::filesystem::path> path_of(std::string_view, std::string_view) const
    {
        return std::filesystem::path{ "resolved/on/disk" };
    }
};

}

TEST_CASE("source_stack resolves top-down and index 0 wins", "[io][stack]")
{
    int infos = 0;
    meios::log_sink_f log{ info_counter{ infos } };
    meios::log_sink &seam = log;

    meios::source_stack stack{
        fixture_source{ "high", { { "pkg", "file" } } },
        fixture_source{ "low", { { "pkg", "file" } } },
    };

    std::optional<meios::resolved_asset> hit = stack.locate("pkg", "file", seam);

    REQUIRE(hit.has_value());
    REQUIRE(hit->path() == std::filesystem::path{ "high" });
    REQUIRE(infos == 1);
}

TEST_CASE("a lower layer answers when higher layers miss, without a shadow log", "[io][stack]")
{
    int infos = 0;
    meios::log_sink_f log{ info_counter{ infos } };
    meios::log_sink &seam = log;

    meios::source_stack stack{
        fixture_source{ "high", { { "other", "file" } } },
        fixture_source{ "low", { { "pkg", "file" } } },
    };

    std::optional<meios::resolved_asset> hit = stack.locate("pkg", "file", seam);

    REQUIRE(hit.has_value());
    REQUIRE(hit->path() == std::filesystem::path{ "low" });
    REQUIRE(infos == 0);
}

TEST_CASE("an unresolved query returns nullopt and logs nothing", "[io][stack]")
{
    int infos = 0;
    meios::log_sink_f log{ info_counter{ infos } };
    meios::log_sink &seam = log;

    meios::source_stack stack{ fixture_source{ "only", { { "pkg", "file" } } } };

    REQUIRE_FALSE(stack.locate("pkg", "missing", seam).has_value());
    REQUIRE(infos == 0);
}

TEST_CASE("source_handle resolves optional capabilities at the erasure boundary", "[io][stack]")
{
    meios::source_handle bytes_only{ fixture_source{ "mem", {} } };
    meios::source_handle path_backed{ path_fixture{} };

    REQUIRE_FALSE(bytes_only.path_of("pkg", "file").has_value());
    REQUIRE(path_backed.path_of("pkg", "file").has_value());
    REQUIRE(path_backed.path_of("pkg", "file").value() == std::filesystem::path{ "resolved/on/disk" });
}

TEST_CASE("a moved-from source_handle reports not valid", "[io][stack]")
{
    meios::source_handle handle{ fixture_source{ "mem", {} } };
    REQUIRE(handle.valid());

    meios::source_handle moved{ std::move(handle) };
    REQUIRE(moved.valid());
    REQUIRE_FALSE(handle.valid());
}

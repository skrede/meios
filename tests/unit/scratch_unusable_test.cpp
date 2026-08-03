#include "scratch_capture.h"

#include <meios/urdf.h>
#include <meios/io.h>
#include <meios/model.h>
#include <meios/xacro.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>

// Every case here runs with the temporary-directory environment pointed at a path the
// build never creates, so a byte-backed source constructed in this binary can never
// obtain a scratch root. That environment is process-wide, which is why this is a
// target of its own rather than a case beside ones that need a real temporary directory.

namespace
{

using scratch_test::event;
using scratch_test::event_log;
using scratch_test::capture;

std::size_t write_failures(const event_log &events)
{
    std::size_t total = 0;
    for(const event &recorded : events)
        if(recorded.lvl == meios::level::error
           && recorded.code == meios::diagnostic_code::asset_write_failed)
            ++total;
    return total;
}

std::filesystem::path owned_dir(const char *name)
{
    const std::filesystem::path path = std::filesystem::current_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    std::filesystem::create_directories(path, ec);
    return path;
}

bool is_empty_dir(const std::filesystem::path &path)
{
    std::error_code ec;
    return std::filesystem::directory_iterator(path, ec) == std::filesystem::directory_iterator();
}

std::filesystem::path mesh_fixture()
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "package_mesh.urdf";
}

}

TEST_CASE("constructing a byte-backed source against an unusable temporary directory does not throw",
          "[io][scratch][unusable]")
{
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;

    REQUIRE_NOTHROW(meios::memory_source{ seam });
    REQUIRE(write_failures(events) == 1);
}

TEST_CASE("the reason a scratch directory could not be obtained reaches the diagnostic",
          "[io][scratch][unusable]")
{
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;

    const meios::memory_source source{ seam };
    REQUIRE(events.size() == 1);
    // The wording checked is meios's own, not the platform's: what is asserted is that a
    // reason was appended at all, since the message text a system carries is its own.
    const std::size_t reason = events.front().text.find("byte-backed source: ");
    REQUIRE(reason != std::string::npos);
    REQUIRE(events.front().text.size() > reason + std::string("byte-backed source: ").size());
}

TEST_CASE("the precondition that failed reaches the diagnostic as a structured cause",
          "[io][scratch][unusable]")
{
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;

    const meios::memory_source source{ seam };
    REQUIRE(events.size() == 1);
    REQUIRE(events.front().cause.has_value());
    REQUIRE(events.front().cause->operation == meios::operation_kind::status);
}

TEST_CASE("a source that owns no scratch serves nothing", "[io][scratch][unusable]")
{
    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;

    meios::memory_source source{ seam };
    source.add("somepkg", "meshes/x.stl", "solid\n");

    REQUIRE_FALSE(source.locate("somepkg", "meshes/x.stl").has_value());
    REQUIRE(write_failures(events) == 2);
}

TEST_CASE("a source that owns no scratch writes nothing beside the process",
          "[io][scratch][unusable]")
{
    const std::filesystem::path previous = std::filesystem::current_path();
    const std::filesystem::path here = owned_dir("scratch-unusable-cwd");
    std::filesystem::current_path(here);

    event_log events;
    meios::log_sink_f log{ capture{ events } };
    meios::log_sink &seam = log;
    meios::memory_source source{ seam };
    source.add("somepkg", "meshes/x.stl", "solid\n");
    const bool served = source.locate("somepkg", "meshes/x.stl").has_value();
    const bool untouched = is_empty_dir(here);

    std::filesystem::current_path(previous);
    REQUIRE(untouched);
    REQUIRE_FALSE(served);
}

TEST_CASE("a load through a source that owns no scratch is refused at every asset policy",
          "[io][scratch][unusable]")
{
    meios::log_sink inner;
    meios::capturing_log_sink log{ inner };
    meios::memory_source source{ log };
    source.add("somepkg", "meshes/x.stl", "solid\n");
    meios::source_stack sources{ std::move(source) };

    for(meios::missing_asset policy : { meios::missing_asset::skip, meios::missing_asset::warn,
                                        meios::missing_asset::fail })
    {
        INFO("missing_asset " << static_cast<int>(policy));
        meios::load_options opts;
        opts.on_missing = policy;
        const meios::expected<meios::load_result, meios::load_error> result =
            meios::load(mesh_fixture(), opts, sources, log);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == meios::diagnostic_code::asset_write_failed);
    }
}

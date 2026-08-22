#include "acquisition_script.h"
#include "meios/urdf/load_acquire.h"

#include <meios/io/source_stack.h>
#include <meios/io/source_lookup.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

class stage_trace final : public meios::detail::load_stage_probe
{
public:
    void entered(meios::detail::load_stage_point point) override
    {
        points.push_back(point);
    }

    std::vector<meios::detail::load_stage_point> points;
};

struct failing_source
{
    meios::capability_descriptor capabilities() const
    {
        return descriptor;
    }

    std::optional<meios::resolved_asset> locate(std::string_view package, std::string_view relative) const
    {
        meios::source_lookup_result hit = try_locate(package, relative);
        return hit ? std::move(*hit) : std::nullopt;
    }

    meios::source_lookup_result try_locate(std::string_view, std::string_view) const
    {
        return meios::unexpected<meios::operation_failure>(failure);
    }

    meios::capability_descriptor descriptor{meios::source_kind::memory, false};
    meios::operation_failure failure{meios::operation_kind::canonicalize, {43, acquisition_test::category}};
};

std::filesystem::path fixture(const std::string &name)
{
    return std::filesystem::path{MEIOS_URDF_FIXTURE_DIR} / name;
}

void require_failure(acquisition_test::reader_script script, meios::operation_kind operation, int value)
{
    acquisition_test::scripted_operations operations{script};
    meios::log_sink silent;
    meios::capturing_log_sink capture{silent};
    meios::capture_window window{capture};
    meios::source_stack sources;
    meios::load_options options;
    stage_trace trace;
    const auto result = meios::detail::drive_load("unreadable.urdf", options, sources, window, operations, trace);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(trace.points.empty());
    REQUIRE(result.error().diagnostics.size() == 1);
    REQUIRE(result.error().cause.has_value());
    REQUIRE(result.error().diagnostics.front().cause.has_value());
    REQUIRE(result.error().cause->operation == operation);
    REQUIRE(result.error().cause->native.value() == value);
    if(value != 0)
        REQUIRE(&result.error().cause->native.category() == &acquisition_test::category);
    REQUIRE(result.error().diagnostics.front().cause->operation == result.error().cause->operation);
    REQUIRE(result.error().diagnostics.front().cause->native == result.error().cause->native);
}

}

TEST_CASE("every top-level acquisition failure stops before downstream stages", "[urdf][load_acquisition]")
{
    acquisition_test::reader_script status;
    status.status_error = {11, acquisition_test::category};
    require_failure(status, meios::operation_kind::status, 11);

    acquisition_test::reader_script non_regular;
    non_regular.status = std::filesystem::file_status(std::filesystem::file_type::directory);
    require_failure(non_regular, meios::operation_kind::status, 0);

    acquisition_test::reader_script open;
    open.open_error = {13, acquisition_test::category};
    require_failure(open, meios::operation_kind::open, 13);

    acquisition_test::reader_script read;
    read.read_error = {17, acquisition_test::category};
    require_failure(read, meios::operation_kind::read, 17);

    acquisition_test::reader_script close;
    close.close_error = {19, acquisition_test::category};
    require_failure(close, meios::operation_kind::close, 19);
}

TEST_CASE("a readable description enters sniff drive and assemble in order", "[urdf][load_acquisition]")
{
    meios::log_sink silent;
    meios::capturing_log_sink capture{silent};
    meios::capture_window window{capture};
    meios::source_stack sources;
    meios::load_options options;
    stage_trace trace;
    const auto result = meios::detail::drive_load(fixture("test-desc.urdf"), options, sources, window, meios::detail::default_text_reader_operations(), trace);

    REQUIRE(result.has_value());
    REQUIRE(result->robot.name == "test");
    REQUIRE(trace.points == std::vector{meios::detail::load_stage_point::sniff, meios::detail::load_stage_point::drive, meios::detail::load_stage_point::assemble});
}

TEST_CASE("an asset acquisition cause survives downstream assembly", "[urdf][load_acquisition]")
{
    meios::log_sink silent;
    meios::capturing_log_sink capture{silent};
    meios::capture_window window{capture};
    meios::source_stack sources{failing_source{}};
    meios::load_options options;
    stage_trace trace;
    const auto result = meios::detail::drive_load(fixture("package_mesh.urdf"), options, sources, window, meios::detail::default_text_reader_operations(), trace);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().cause.has_value());
    REQUIRE(result.error().cause->native.value() == 43);
    REQUIRE(result.error().cause->operation == result.error().diagnostics.front().cause->operation);
    REQUIRE(result.error().cause->native == result.error().diagnostics.front().cause->native);
}

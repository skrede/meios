#include "source_lookup_fixture.h"

#include "verbs/counting_log_sink.h"

#include <meios/xacro.h>

#include <meios/sink/world_recorder.h>

#include <meios/io/source_stack.h>

#include <meios/records/link.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/capturing_log_sink.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include <string_view>

namespace
{
using acquisition_test::record;
using acquisition_test::recorder;

const meios::operation_failure native{meios::operation_kind::open, {41, acquisition_test::lookup_error_category}};

constexpr std::string_view document = "<robot xmlns:xacro=\"http://ros.org/wiki/xacro\"><l>${x}</l></robot>";

// Refuses every expression outright, first reporting through the sink the substitution
// layer handed it, so the adapters between that sink and the caller's are exercised by
// the production path rather than reproduced here.
struct refusing_backend
{
    bool with_cause;

    std::optional<std::string> eval_to_text(std::string_view, const meios::eval_scope &, meios::log_sink &log) const
    {
        if(with_cause)
            log.log(meios::level::error, meios::diagnostic_code::cannot_open, meios::source_location{}, native, "backend refused");
        else
            log.log(meios::level::error, "backend refused");
        return std::nullopt;
    }

    static meios::eval_failure_kind last_failure_kind()
    {
        return meios::eval_failure_kind::error;
    }
};

std::vector<record> expand_through(bool with_cause, meios::eval_policy policy)
{
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::eval_scope scope;
    meios::source_stack sources;
    const auto backend = std::make_shared<meios::evaluator_handle>(refusing_backend{with_cause});
    meios::expand(document, scope, sources, "robot.xacro", meios::expansion_limits{}, policy, backend, log);
    return records;
}

std::optional<record> single_cause(const std::vector<record> &records)
{
    std::optional<record> only;
    for(const record &noted : records)
    {
        if(!noted.cause)
            continue;
        if(only)
            return std::nullopt;
        only = noted;
    }
    return only;
}

void require_native_cause(const std::vector<record> &records)
{
    const std::optional<record> carried = single_cause(records);
    REQUIRE(carried.has_value());
    REQUIRE(carried->code == meios::diagnostic_code::cannot_open);
    REQUIRE(carried->cause->operation == native.operation);
    REQUIRE(carried->cause->native == native.native);
    REQUIRE(carried->cause->native.value() == 41);
    REQUIRE(&carried->cause->native.category() == &acquisition_test::lookup_error_category);
}

meios::link<double> located_link(const std::string &name, int line)
{
    meios::link<double> node;
    node.name       = name;
    node.origin_loc = meios::source_location{"robot.urdf", line, 1};
    return node;
}

}

TEST_CASE("buffered substitution replay preserves the native cause", "[operation_adapter]")
{
    require_native_cause(expand_through(true, meios::eval_policy::skip));
    require_native_cause(expand_through(true, meios::eval_policy::warn));
}

TEST_CASE("loud substitution relocation forwards the native cause unchanged", "[operation_adapter]")
{
    require_native_cause(expand_through(true, meios::eval_policy::fail));
}

TEST_CASE("a cause-free refusal replays without a fabricated cause", "[operation_adapter]")
{
    const std::vector<record> records = expand_through(false, meios::eval_policy::skip);
    const auto invented               = [](const record &noted) { return noted.cause.has_value(); };
    const auto relocated              = [](const record &noted) { return noted.code == meios::diagnostic_code::expression_error; };

    REQUIRE_FALSE(records.empty());
    REQUIRE(std::none_of(records.begin(), records.end(), invented));
    REQUIRE(std::any_of(records.begin(), records.end(), relocated));
}

TEST_CASE("counting forwards one exact cause and counts it once", "[operation_adapter]")
{
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::capturing_log_sink capture{log};
    meios::cli::counting_log_sink counting{capture};
    meios::log_sink &seam = counting;

    seam.log(meios::level::error, meios::diagnostic_code::cannot_open, {}, native, "open failed");

    REQUIRE(counting.errors() == 1);
    REQUIRE(capture.size() == 1);
    REQUIRE(capture.first()->cause->operation == native.operation);
    REQUIRE(capture.first()->cause->native == native.native);
    require_native_cause(records);
}

TEST_CASE("world first-error capture keeps the first located failure once", "[operation_adapter]")
{
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::world_recorder world{log, meios::topology_policy::fail};
    world.on_link(located_link("base", 3));
    world.on_link(located_link("stray", 7));
    world.finish();

    REQUIRE_FALSE(world.ok());
    REQUIRE(world.first_failure().has_value());
    REQUIRE(world.first_failure()->line == 7);
    REQUIRE(records.size() == 1);
    REQUIRE(records.front().code == meios::diagnostic_code::additional_root);
    REQUIRE_FALSE(records.front().cause.has_value());
}

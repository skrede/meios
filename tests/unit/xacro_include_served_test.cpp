#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/urdf.h>
#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/operation_failure.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <string_view>

using include_tree::robot;
using include_tree::binds;
using include_tree::include;
using include_tree::reads_leaf;
using include_tree::expand_text;
using include_tree::renders_link;
using include_tree::expansion_result;

namespace
{

using request = std::pair<std::string, std::string>;

const std::string refusal = "a test layer refused the request";

struct note_recorder
{
    std::vector<std::string> &infos;

    void keep(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::info)
            infos.push_back(message);
    }

    void operator()(meios::level lvl, const std::string &message) { keep(lvl, message); }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message) { keep(lvl, message); }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &, const std::string &message) { keep(lvl, message); }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &, const meios::operation_failure &, const std::string &message) { keep(lvl, message); }
};

// Refuses every request through the sink it was built with, as the built-in layers refuse.
struct refusing_layer
{
    std::reference_wrapper<meios::log_sink> log;
    std::reference_wrapper<std::vector<request>> asked;

    static meios::capability_descriptor capabilities() { return { meios::source_kind::directory, false }; }

    std::optional<meios::resolved_asset> locate(std::string_view package, std::string_view relative)
    {
        asked.get().emplace_back(std::string(package), std::string(relative));
        log.get().log(meios::level::error, meios::diagnostic_code::uncontained_asset, meios::source_location{}, refusal);
        return std::nullopt;
    }
};

meios::memory_source sibling_pair(meios::log_sink &log, std::string_view leaf)
{
    meios::memory_source held{ log };
    held.add("pkg", "urdf/a.xacro", robot(include("b.xacro")));
    held.add("pkg", "urdf/b.xacro", binds(leaf));
    return held;
}

expansion_result expand_top(std::string_view includes, meios::source_stack &sources, meios::log_sink &log)
{
    return expand_text(robot(std::string(includes) + "<link name=\"${leaf}\"/>"), std::filesystem::path{}, sources, log);
}

const std::string top_a = include("package://pkg/urdf/a.xacro");

}

TEST_CASE("a relative include in a document a byte source served resolves through that source", "[xacro][structural][include][served]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ sibling_pair(log, "sibling") };

    REQUIRE(renders_link(expand_top(top_a, sources, log), "sibling"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a served document's sibling resolves the same whether or not an earlier include wrote it", "[xacro][structural][include][served]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack alone{ sibling_pair(log, "sibling") };
    meios::source_stack after{ sibling_pair(log, "sibling") };

    const expansion_result first = expand_top(top_a, alone, log);
    const expansion_result second = expand_top(include("package://pkg/urdf/b.xacro") + top_a, after, log);

    REQUIRE(renders_link(first, "sibling"));
    REQUIRE(renders_link(second, "sibling"));
    REQUIRE(first->document == second->document);
}

TEST_CASE("a relative include nested in a served document stays in the serving package", "[xacro][structural][include][served]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::memory_source held{ log };
    held.add("pkg", "urdf/a.xacro", robot(include("sub/c.xacro")));
    held.add("pkg", "urdf/sub/c.xacro", robot(include("d.xacro")));
    held.add("pkg", "urdf/sub/d.xacro", binds("nested"));
    held.add("pkg", "urdf/d.xacro", binds("wrong"));
    meios::source_stack sources{ std::move(held) };

    REQUIRE(renders_link(expand_top(top_a, sources, log), "nested"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("the layer that served a document answers for its sibling over a higher layer's copy", "[xacro][structural][include][served]")
{
    const include_tree::tree higher;
    higher.write("pkg/urdf/b.xacro", binds("higher"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ higher.root(), log }, sibling_pair(log, "serving") };

    REQUIRE(renders_link(expand_top(top_a, sources, log), "serving"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a sibling the serving layer lacks falls back to the configured sources", "[xacro][structural][include][served]")
{
    const include_tree::tree fallback;
    fallback.write("z.xacro", binds("fallback"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::memory_source held{ log };
    held.add("pkg", "urdf/a.xacro", robot(include("z.xacro")));
    meios::source_stack sources{ std::move(held), meios::directory_source{ fallback.root(), log } };

    REQUIRE(renders_link(expand_top(top_a, sources, log), "fallback"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a sibling asked of its serving layer leaves the shadow notes as the stack logs them", "[xacro][structural][include][served]")
{
    std::vector<std::string> logged;
    meios::log_sink_f log{ note_recorder{ logged } };
    meios::source_stack sources{ sibling_pair(log, "serving"), sibling_pair(log, "lower") };
    std::vector<std::string> expected;
    meios::log_sink_f stack_log{ note_recorder{ expected } };
    meios::source_stack plain{ sibling_pair(stack_log, "serving"), sibling_pair(stack_log, "lower") };
    REQUIRE(plain.locate("pkg", "urdf/a.xacro", stack_log).has_value());

    REQUIRE(renders_link(expand_top(top_a, sources, log), "serving"));
    REQUIRE(expected.size() == 1);
    REQUIRE(logged == expected);
}

TEST_CASE("a layer's refusal reaches a refusing load's diagnostics once when a lower layer serves the document", "[xacro][structural][include][served][load]")
{
    const include_tree::tree held;
    const std::filesystem::path top = held.write("top.xacro", reads_leaf("package://pkg/urdf/a.xacro"));
    meios::log_sink inner;
    meios::capturing_log_sink capture{ inner };
    std::vector<request> asked;
    meios::source_stack sources{ refusing_layer{ capture, asked }, sibling_pair(capture, "sibling") };

    const meios::expected<meios::load_result, meios::load_error> result = meios::load(top, meios::load_options{}, sources, capture);
    const std::vector<meios::captured_diagnostic> records = capture.records();

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::uncontained_asset);
    REQUIRE(asked == std::vector<request>{ { "pkg", "urdf/a.xacro" } });
    REQUIRE(std::count_if(records.begin(), records.end(), [](const meios::captured_diagnostic &r) { return r.message == refusal; }) == 1);
}

#include "source_lookup_fixture.h"

#include "verbs/verbs.h"
#include "verbs/deferred_error_sink.h"

#include <meios/urdf/load.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/load_error.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/capturing_log_sink.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <ostream>
#include <sstream>
#include <utility>
#include <iostream>
#include <filesystem>
#include <system_error>

namespace
{
using meios::cli::verb_context;

// Redirects one standard stream for the scope of a verb call so a case can assert on the
// bytes that verb wrote, restoring the stream on destruction.
class stream_capture
{
public:
    explicit stream_capture(std::ostream &stream)
            : m_buffer()
            , m_stream(stream)
            , m_saved(stream.rdbuf(m_buffer.rdbuf()))
    {
    }

    ~stream_capture()
    {
        m_stream.rdbuf(m_saved);
    }

    stream_capture(const stream_capture &)            = delete;
    stream_capture &operator=(const stream_capture &) = delete;

    std::string str() const
    {
        return m_buffer.str();
    }

private:
    std::ostringstream m_buffer;
    std::ostream &m_stream;
    std::streambuf *m_saved;
};

// Removes a seeded tree even when a REQUIRE aborts the case, so one failing row cannot
// leave the next run resolving against a directory it did not create.
class scoped_tree
{
public:
    explicit scoped_tree(std::filesystem::path root)
            : m_root(std::move(root))
    {
        std::filesystem::remove_all(m_root);
        std::filesystem::create_directories(m_root);
    }

    ~scoped_tree()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_root, ec);
    }

    scoped_tree(const scoped_tree &)            = delete;
    scoped_tree &operator=(const scoped_tree &) = delete;

    const std::filesystem::path &path() const
    {
        return m_root;
    }

private:
    std::filesystem::path m_root;
};

struct verb_row
{
    std::string name;
    int (*run)(const verb_context &);
};

const std::vector<verb_row> emitting_verbs = {{"flatten", &meios::cli::run_flatten}, {"bundle", &meios::cli::run_bundle}, {"deps", &meios::cli::run_deps}};

std::size_t occurrences(const std::string &haystack, const std::string &needle)
{
    std::size_t count = 0;
    for(std::size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + needle.size()))
        ++count;
    return count;
}

void require_single_refusal(const verb_row &verb, const std::filesystem::path &input)
{
    verb_context ctx;
    ctx.id                    = verb.name;
    ctx.positionals           = {input.string()};
    ctx.value_flags["--name"] = "acquisition-bundle";

    stream_capture out{std::cout};
    stream_capture err{std::cerr};
    const int code             = verb.run(ctx);
    const std::string printed  = out.str();
    const std::string reported = err.str();

    INFO(verb.name << " over " << input.string());
    REQUIRE(code != 0);
    REQUIRE(printed.empty());
    REQUIRE(occurrences(reported, "(cannot_open)") == 1);
    REQUIRE(reported.find("parse error") == std::string::npos);
    REQUIRE(reported.find("<robot>") == std::string::npos);
}

}

TEST_CASE("a missing top-level input refuses once through every emitting verb", "[cli_acquisition]")
{
    const scoped_tree tree{std::filesystem::temp_directory_path() / "meios_cli_acquisition_absent"};
    for(const verb_row &verb : emitting_verbs)
        require_single_refusal(verb, tree.path() / "absent.urdf");
}

TEST_CASE("a non-regular top-level input refuses once through every emitting verb", "[cli_acquisition]")
{
    const scoped_tree tree{std::filesystem::temp_directory_path() / "meios_cli_acquisition_folder"};
    const std::filesystem::path folder = tree.path() / "model.urdf";
    std::filesystem::create_directories(folder);
    for(const verb_row &verb : emitting_verbs)
        require_single_refusal(verb, folder);
}

TEST_CASE("a warning preceding an acquisition failure survives it exactly once", "[cli_acquisition]")
{
    const scoped_tree tree{std::filesystem::temp_directory_path() / "meios_cli_acquisition_warned"};
    std::ostringstream stream;
    meios::log_sink_s terminal{stream};
    meios::cli::deferred_error_sink sink{terminal};
    meios::capturing_log_sink capture{sink};
    capture.log(meios::level::warn, meios::diagnostic_code::unresolved_asset, {}, "a source declined an asset");

    meios::source_stack sources;
    const meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> loaded = meios::load(tree.path() / "absent.urdf", opts, sources, capture);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error().diagnostics.size() == 1);
    REQUIRE(sink.errors() == 0);
    meios::cli::relay_failure(loaded.error(), terminal);

    const std::string reported = stream.str();
    REQUIRE(occurrences(reported, "(unresolved_asset)") == 1);
    REQUIRE(occurrences(reported, "(cannot_open)") == 1);
}

TEST_CASE("a deferred error replays its exact native cause and no other", "[cli_acquisition]")
{
    const meios::operation_failure cause{meios::operation_kind::open, {41, acquisition_test::lookup_error_category}};
    std::vector<acquisition_test::record> records;
    meios::log_sink_f terminal{acquisition_test::recorder{records}};
    meios::capturing_log_sink capture{terminal};
    meios::cli::deferred_error_sink sink{capture};
    meios::log_sink &seam = sink;

    seam.log(meios::level::warn, meios::diagnostic_code::unresolved_asset, {}, "soft");
    seam.log(meios::level::error, meios::diagnostic_code::cannot_open, {}, cause, "hard");
    REQUIRE(sink.errors() == 1);
    REQUIRE(records.size() == 1);
    REQUIRE_FALSE(records.front().cause.has_value());

    sink.replay_first(capture);
    REQUIRE(records.size() == 2);
    REQUIRE(records.back().code == meios::diagnostic_code::cannot_open);
    REQUIRE(records.back().cause->operation == cause.operation);
    REQUIRE(records.back().cause->native == cause.native);
    REQUIRE(&records.back().cause->native.category() == &acquisition_test::lookup_error_category);

    meios::cli::deferred_error_sink legacy{capture};
    static_cast<meios::log_sink &>(legacy).log(meios::level::error, meios::diagnostic_code::xml_parse_error, {}, "legacy");
    legacy.replay_first(capture);
    REQUIRE(records.size() == 3);
    REQUIRE_FALSE(records.back().cause.has_value());
}

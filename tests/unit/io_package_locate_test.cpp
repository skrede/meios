#include "source_lookup_fixture.h"

#include <meios/io/bundle_source.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace
{
using acquisition_test::loop_root;
using acquisition_test::make_tree;

struct worded
{
    meios::level level;
    meios::diagnostic_code code;
    std::optional<meios::operation_failure> cause;
    std::string message;
};

struct wording_recorder
{
    void operator()(meios::level level, const std::string &message)
    {
        records.push_back({level, meios::diagnostic_code::unspecified, std::nullopt, message});
    }

    void operator()(meios::level level, const meios::source_location &, const std::string &message)
    {
        records.push_back({level, meios::diagnostic_code::unspecified, std::nullopt, message});
    }

    void operator()(meios::level level, meios::diagnostic_code code, const meios::source_location &, const std::string &message)
    {
        records.push_back({level, code, std::nullopt, message});
    }

    void operator()(meios::level level, meios::diagnostic_code code, const meios::source_location &, const meios::operation_failure &cause, const std::string &message)
    {
        records.push_back({level, code, cause, message});
    }

    std::vector<worded> &records;
};

template<typename Source>
void require_undecided_logged(const std::filesystem::path &root)
{
    std::vector<worded> records;
    meios::log_sink_f log{wording_recorder{records}};
    Source source{root, log};
    REQUIRE_FALSE(source.locate("pkg", "x.stl").has_value());
    REQUIRE(records.size() == 1);
    REQUIRE(records.front().level == meios::level::error);
    REQUIRE(records.front().code == meios::diagnostic_code::uncontained_asset);
    REQUIRE(records.front().cause.has_value());
    REQUIRE(records.front().cause->operation == meios::operation_kind::canonicalize);
    REQUIRE(records.front().message.find("escapes") == std::string::npos);
}

}

TEST_CASE("a package lookup that cannot decide containment logs the cause, not an escape", "[io][lookup]")
{
    const meios::scratch_dir tree    = make_tree();
    const std::filesystem::path loop = loop_root(tree.path());
    require_undecided_logged<meios::directory_source>(loop);
    require_undecided_logged<meios::bundle_source>(loop);

    std::vector<worded> records;
    meios::log_sink_f log{wording_recorder{records}};
    meios::directory_source directory{loop, log};
    const meios::source_lookup_result typed = directory.try_locate("pkg", "x.stl");
    REQUIRE_FALSE(typed.has_value());
    REQUIRE(typed.error().operation == meios::operation_kind::canonicalize);
    REQUIRE(records.empty());
}

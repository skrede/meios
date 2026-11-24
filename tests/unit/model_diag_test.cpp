#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/capturing_log_sink.h>
#include <meios/diagnostic/captured_diagnostic.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <sstream>
#include <optional>

namespace
{

struct capture
{
    meios::level lvl{};
    std::optional<meios::source_location> location{};
    std::string message{};
    int calls{0};
};

struct recorder
{
    capture &target;

    void operator()(meios::level lvl, const std::string &message)
    {
        target.lvl = lvl;
        target.location.reset();
        target.message = message;
        ++target.calls;
    }

    void operator()(meios::level lvl, const meios::source_location &location, const std::string &message)
    {
        target.lvl      = lvl;
        target.location = location;
        target.message  = message;
        ++target.calls;
    }
};

}

TEST_CASE("to_string renders level and document position", "[model][diag]")
{
    REQUIRE(meios::to_string(meios::level::error) == "error");
    REQUIRE(meios::to_string(meios::level::warn) == "warn");
    REQUIRE(meios::to_string(meios::level::info) == "info");
    REQUIRE(meios::to_string(meios::source_location{"robot.urdf", 12, 4}) == "robot.urdf:12:4");
}

TEST_CASE("default log_sink is a silent no-op on both overloads", "[model][diag]")
{
    meios::log_sink sink;
    meios::log_sink &seam = sink;
    REQUIRE_NOTHROW(seam.log(meios::level::error, "unlocated"));
    REQUIRE_NOTHROW(seam.log(meios::level::warn, meios::source_location{"a.xml", 3, 7}, "located"));
}

TEST_CASE("log_sink_f forwards both overloads through the base seam", "[model][diag]")
{
    capture state;
    meios::log_sink_f adapter{recorder{state}};
    meios::log_sink &seam = adapter;

    seam.log(meios::level::info, "hello");
    REQUIRE(state.calls == 1);
    REQUIRE(state.lvl == meios::level::info);
    REQUIRE_FALSE(state.location.has_value());
    REQUIRE(state.message == "hello");

    seam.log(meios::level::error, meios::source_location{"robot.urdf", 12, 4}, "boom");
    REQUIRE(state.calls == 2);
    REQUIRE(state.lvl == meios::level::error);
    REQUIRE(state.location.has_value());
    REQUIRE(meios::to_string(*state.location) == "robot.urdf:12:4");
    REQUIRE(state.message == "boom");
}

TEST_CASE("log_sink_s prefixes level and document position", "[model][diag]")
{
    std::ostringstream out;
    meios::log_sink_s adapter{out};
    meios::log_sink &seam = adapter;

    seam.log(meios::level::warn, "plain");
    seam.log(meios::level::error, meios::source_location{"a.xml", 3, 7}, "located");
    REQUIRE(out.str() == "[warn] plain\na.xml:3:7: [error] located\n");
}

TEST_CASE("a mark from one sink asks another for nothing it does not hold", "[model][diag]")
{
    meios::log_sink discard;
    meios::capturing_log_sink busy{discard};
    meios::log_sink &seam = busy;
    seam.log(meios::level::info, "first");
    seam.log(meios::level::warn, "second");
    seam.log(meios::level::error, "third");

    const std::size_t elsewhere = busy.size();
    meios::capturing_log_sink fresh{discard};

    REQUIRE(elsewhere > fresh.size());
    REQUIRE(fresh.records(elsewhere).empty());
    REQUIRE(fresh.errors(elsewhere) == 0);
    REQUIRE_FALSE(fresh.first(elsewhere).has_value());
}

TEST_CASE("a mark the sink does hold still answers for what followed it", "[model][diag]")
{
    meios::log_sink discard;
    meios::capturing_log_sink sink{discard};
    meios::log_sink &seam = sink;
    seam.log(meios::level::info, "before the mark");

    const std::size_t taken = sink.size();
    seam.log(meios::level::warn, "after the mark");
    seam.log(meios::level::error, "also after the mark");

    const std::vector<meios::captured_diagnostic> since = sink.records(taken);
    REQUIRE(since.size() == 2);
    REQUIRE(since.front().message == "after the mark");
    REQUIRE(since.back().message == "also after the mark");
    REQUIRE(sink.records().size() == 3);
}

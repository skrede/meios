#include "xacro_read_probe.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

const char *head()
{
    return "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">";
}

// A macro whose body instantiates itself: the cycle a fuzzer found, which reaches the walk
// through dispatch, the macro's own children and the emit of the element wrapping the call.
const char *self_instantiating()
{
    return "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
           "<xacro:macro name=\"loop\"><a><xacro:loop/></a></xacro:macro>"
           "<xacro:loop/></robot>";
}

// The root element is itself one level, so a document with `levels` nested children stands
// `levels + 1` deep at its innermost node.
std::string nested(std::size_t levels)
{
    std::string document = head();
    for(std::size_t at = 0; at < levels; ++at)
        document += "<a>";
    for(std::size_t at = 0; at < levels; ++at)
        document += "</a>";
    return document + "</robot>";
}

expansion_result run(const std::string &source, const meios::expansion_limits &limits,
                     std::vector<xacro_probe::captured> &records)
{
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, "robot.xacro", limits, log);
}

expansion_result run(const std::string &source, const meios::expansion_limits &limits)
{
    std::vector<xacro_probe::captured> records;
    return run(source, limits, records);
}

}

TEST_CASE("a macro instantiating itself refuses instead of taking the native stack",
          "[xacro][budget][depth]")
{
    std::vector<xacro_probe::captured> records;
    const expansion_result out = run(self_instantiating(), meios::expansion_limits{}, records);

    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::expansion_budget_exceeded);
    CHECK(out.error().message.find("expansion-depth") != std::string::npos);
    CHECK_FALSE(out.error().loc.file.empty());
    CHECK(xacro_probe::errors(records) == 1);
}

TEST_CASE("nesting at the ceiling expands and one level past it refuses",
          "[xacro][budget][depth]")
{
    const std::size_t ceiling = meios::expansion_limits{}.depth;

    CHECK(run(nested(ceiling - 1), meios::expansion_limits{}).has_value());
    CHECK_FALSE(run(nested(ceiling), meios::expansion_limits{}).has_value());
}

TEST_CASE("the boundary moves with a ceiling a caller raises or lowers",
          "[xacro][budget][depth]")
{
    const std::string document = nested(40);

    CHECK(run(document, meios::expansion_limits{ 1'000'000, 100'000, 41 }).has_value());
    CHECK_FALSE(run(document, meios::expansion_limits{ 1'000'000, 100'000, 40 }).has_value());
    CHECK(run(document, meios::expansion_limits{ 1'000'000, 100'000, 4'000 }).has_value());
}

// The evaluator's ceilings read a zero as a request for the default, so no spelling there means
// unbounded. The expansion ceilings have never carried that rule -- a zero work ceiling refuses
// the first node -- and the depth axis follows the two beside it rather than the other struct.
TEST_CASE("a zero expansion ceiling is a crossed ceiling rather than an absent one",
          "[xacro][budget][depth]")
{
    CHECK(meios::expansion_limits{ 1'000'000, 100'000, 0 }.depth == 0);
    CHECK_FALSE(run(nested(1), meios::expansion_limits{ 1'000'000, 100'000, 0 }).has_value());
}

TEST_CASE("the refusal is terminal under every evaluation policy", "[xacro][budget][depth]")
{
    for(meios::eval_policy policy :
        { meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip })
    {
        std::vector<xacro_probe::captured> records;
        meios::log_sink_f log{ xacro_probe::recorder{ records } };
        meios::source_stack sources;
        meios::eval_scope scope;

        const expansion_result out =
            meios::expand(self_instantiating(), scope, sources, "robot.xacro",
                          meios::expansion_limits{}, policy, {}, log);

        REQUIRE_FALSE(out.has_value());
        CHECK(out.error().code == meios::diagnostic_code::expansion_budget_exceeded);
    }
}

// The deepest of the seventeen pinned corpus entry points stands eleven levels deep, so the
// ceiling admits an order of magnitude more nesting than any description measured here reaches.
TEST_CASE("a description as deep as the deepest pinned one expands with headroom to spare",
          "[xacro][budget][depth]")
{
    CHECK(run(nested(10), meios::expansion_limits{}).has_value());
    CHECK(meios::expansion_limits{}.depth >= 110);
}

TEST_CASE("the two volume axes keep their meaning beside the new one", "[xacro][budget][depth]")
{
    CHECK(meios::expansion_limits{}.work == 1'000'000);
    CHECK(meios::expansion_limits{}.output_nodes == 100'000);
    CHECK(meios::expansion_limits{}.depth == meios::default_expansion_depth_ceiling);

    CHECK(meios::expansion_limits{ 7 }.work == 7);
    CHECK(meios::expansion_limits{ 7 }.output_nodes == 100'000);
    CHECK(meios::expansion_limits{ 7, 9 }.output_nodes == 9);
    CHECK(meios::expansion_limits{ 7, 9 }.depth == meios::default_expansion_depth_ceiling);
}

#include "xacro_read_probe.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <utility>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

const char *fanout_doc()
{
    return "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
           "<xacro:macro name=\"trio\"><link/><link/><link/></xacro:macro>"
           "<xacro:trio/><xacro:trio/><xacro:trio/></robot>";
}

}

TEST_CASE("a shallow-but-wide macro fan-out trips the output-node budget", "[xacro][budget]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;

    const expansion_result out = meios::expand(fanout_doc(), scope, sources, "robot.xacro",
                                         meios::expansion_limits{ 1'000'000, 4 }, log);

    REQUIRE_FALSE(out.has_value());
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::expansion_budget_exceeded) == 1);
}

TEST_CASE("a low work ceiling halts expansion with a resource diagnostic", "[xacro][budget]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;

    const expansion_result out = meios::expand(fanout_doc(), scope, sources, "robot.xacro",
                                         meios::expansion_limits{ 3, 1'000'000 }, log);

    REQUIRE_FALSE(out.has_value());
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::expansion_budget_exceeded) == 1);
}

TEST_CASE("a generous budget lets an ordinary document expand", "[xacro][budget]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;

    const expansion_result out = meios::expand(fanout_doc(), scope, sources, "robot.xacro",
                                         meios::expansion_limits{}, log);

    REQUIRE(out.has_value());
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::expansion_budget_exceeded) == 0);
}

TEST_CASE("a mutual xacro:include chain is caught by the cycle guard", "[xacro][budget][cycle]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };

    meios::memory_source parts{ log };
    parts.add("pkg", "a.xacro",
              "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
              "<xacro:include filename=\"$(find pkg)/b.xacro\"/></robot>");
    parts.add("pkg", "b.xacro",
              "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
              "<xacro:include filename=\"$(find pkg)/a.xacro\"/></robot>");
    meios::source_stack sources{ std::move(parts) };
    meios::eval_scope scope;

    const char *top = "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                      "<xacro:include filename=\"$(find pkg)/a.xacro\"/></robot>";
    const expansion_result out =
        meios::expand(top, scope, sources, "top.xacro", meios::expansion_limits{}, log);

    REQUIRE_FALSE(out.has_value());
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::xacro_structural_error) == 1);
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::expansion_budget_exceeded) == 0);
}

TEST_CASE("an xacro:include escaping the source root is rejected loudly", "[xacro][budget][root]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::memory_source parts{ log };
    parts.add("pkg", "robot.xacro", "<robot/>");
    meios::source_stack sources{ std::move(parts) };
    meios::eval_scope scope;

    const char *top = "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                      "<xacro:include filename=\"$(find pkg)/../secret.xacro\"/></robot>";
    const expansion_result out =
        meios::expand(top, scope, sources, "top.xacro", meios::expansion_limits{}, log);

    REQUIRE_FALSE(out.has_value());
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::xacro_structural_error) == 1);
}

TEST_CASE("two independent budget-exhausting constructs still report exactly one failure",
          "[xacro][budget][cardinality]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;

    // Two sibling fan-outs each exceed the ceiling on their own; the already-failed
    // short-circuit at the head of the charge is what keeps the second one silent.
    const char *twice = "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                        "<xacro:macro name=\"trio\"><link/><link/><link/></xacro:macro>"
                        "<a><xacro:trio/><xacro:trio/></a>"
                        "<b><xacro:trio/><xacro:trio/></b></robot>";
    const expansion_result out = meios::expand(twice, scope, sources, "robot.xacro",
                                         meios::expansion_limits{ 1'000'000, 4 }, log);

    REQUIRE_FALSE(out.has_value());
    REQUIRE(xacro_probe::errors(records) == 1);
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::expansion_budget_exceeded) == 1);
}

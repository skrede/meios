#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <filesystem>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_beside(const std::string &top, const std::string &included, std::vector<xacro_probe::captured> &records)
{
    const include_tree::tree held;
    held.write("inc.xacro", included);
    const std::filesystem::path document = held.write("top.xacro", top);
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{meios::directory_source{held.root(), log}};
    return include_tree::expand_file(document, sources, log);
}

std::size_t warnings(const std::vector<xacro_probe::captured> &records)
{
    return static_cast<std::size_t>(std::ranges::count(records, meios::level::warn, &xacro_probe::captured::lvl));
}

}

// Upstream warns "Child elements of a <xacro:include> tag are ignored" and drops them as well.
TEST_CASE("an include's child elements are ignored with no warning here", "[xacro][include][warning]")
{
    std::vector<xacro_probe::captured> heard;
    const expansion_result out =
            expand_beside(include_tree::robot(R"(<xacro:include filename="inc.xacro"><link name="c"/></xacro:include>)"), include_tree::robot(R"(<link name="i"/>)"), heard);
    REQUIRE(include_tree::renders_link(out, "i"));
    REQUIRE_FALSE(include_tree::renders_link(out, "c"));
    REQUIRE(warnings(heard) == 0);
}

// Upstream warns "inconsistent namespace redefinitions for xmlns:foo" and keeps the outer declaration too.
TEST_CASE("an include redeclaring a namespace prefix raises no warning here", "[xacro][include][warning]")
{
    std::vector<xacro_probe::captured> heard;
    const char *top            = R"(<robot name="probe" xmlns:foo="urn:a" xmlns:xacro="http://www.ros.org/wiki/xacro"><xacro:include filename="inc.xacro"/></robot>)";
    const char *included       = R"(<robot name="i" xmlns:foo="urn:b" xmlns:xacro="http://www.ros.org/wiki/xacro"><link name="i"/></robot>)";
    const expansion_result out = expand_beside(top, included, heard);
    REQUIRE(out.has_value());
    REQUIRE(meios::canonical_xml(out->document) == R"(<robot name="probe" xmlns:foo="urn:a"><link name="i"></link></robot>)");
    REQUIRE(warnings(heard) == 0);
}

#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>

using include_tree::robot;
using include_tree::binds;
using include_tree::include;
using include_tree::reads_leaf;
using include_tree::expand_file;
using include_tree::renders_link;

namespace
{

std::filesystem::path nested_tree(const include_tree::tree &held)
{
    held.write("leaf.xacro", binds("top_leaf"));
    held.write("sub/leaf.xacro", binds("sub_leaf"));
    held.write("sub/inner.xacro", robot(include("leaf.xacro")));
    return held.write("top.xacro", reads_leaf("sub/inner.xacro"));
}

}

TEST_CASE("a nested relative include resolves beside the file that writes it", "[xacro][structural][include][precedence]")
{
    const include_tree::tree held;
    const std::filesystem::path top = nested_tree(held);
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    REQUIRE(renders_link(expand_file(top, sources, log), "sub_leaf"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a file beside the including document wins over a same-named file in a configured root", "[xacro][structural][include][precedence]")
{
    const include_tree::tree held;
    const std::filesystem::path top = nested_tree(held);
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{meios::directory_source{held.root(), log}};

    REQUIRE(renders_link(expand_file(top, sources, log), "sub_leaf"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("an include in a macro body resolves beside the document that calls the macro", "[xacro][structural][include][precedence]")
{
    const include_tree::tree held;
    held.write("part.xacro", binds("top_part"));
    held.write("sub/part.xacro", binds("sub_part"));
    held.write("sub/defs.xacro", robot("<xacro:macro name=\"emit\" params=\"\">" + include("part.xacro") + "<link name=\"${leaf}\"/></xacro:macro>"));
    const std::filesystem::path top = held.write("top.xacro", robot(include("sub/defs.xacro") + "<xacro:emit/>"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    REQUIRE(renders_link(expand_file(top, sources, log), "top_part"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("same-named include files in two directories are not a cycle", "[xacro][structural][include][precedence][cycle]")
{
    const include_tree::tree held;
    held.write("common.xacro", robot(include("sub/inner.xacro")));
    held.write("sub/inner.xacro", robot(include("common.xacro")));
    held.write("sub/common.xacro", binds("sub_common"));
    const std::filesystem::path top = held.write("top.xacro", reads_leaf("common.xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    REQUIRE(renders_link(expand_file(top, sources, log), "sub_common"));
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::xacro_structural_error) == 0);
}

TEST_CASE("a document including itself beside itself refuses as a cycle", "[xacro][structural][include][precedence][cycle]")
{
    const include_tree::tree held;
    const std::filesystem::path self = held.write("self.xacro", robot(include("self.xacro")));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    const include_tree::expansion_result out = expand_file(self, sources, log);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message.find("cycle") != std::string::npos);
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::xacro_structural_error) == 1);
}

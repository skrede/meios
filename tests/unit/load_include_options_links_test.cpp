#include "xacro_read_probe.h"
#include "xacro_include_tree.h"
#include "load_include_options_fixture.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>
#include <system_error>

using include_options_fixture::climbing;
using include_options_fixture::following;
using include_options_fixture::expand_with;

namespace
{

bool linked_climb_tree(const include_tree::tree &held)
{
    held.write("doc/x.xacro", include_tree::binds("lexical"));
    held.write("elsewhere/x.xacro", include_tree::binds("through_link"));
    held.write("elsewhere/d/inner.xacro", include_tree::reads_leaf("../x.xacro"));
    std::error_code ec;
    std::filesystem::create_directory_symlink(held.root() / "elsewhere" / "d", held.root() / "doc" / "sub", ec);
    return !ec;
}

}

TEST_CASE("a caller following outside links reads a sibling link that leaves the document's directory", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    const std::filesystem::path outside = held.write("outside/leaf.xacro", include_tree::binds("outside"));
    const std::filesystem::path top     = held.write("doc/top.xacro", include_tree::reads_leaf("leaf.xacro"));
    if(!include_tree::linked(outside, held.root() / "doc" / "leaf.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;

    REQUIRE(include_tree::renders_link(expand_with(top, sources, log, following()), "outside"));
    REQUIRE(xacro_probe::errors(records) == 0);
    const include_tree::expansion_result unset = expand_with(top, sources, log, meios::include_options{});
    REQUIRE_FALSE(unset.has_value());
    REQUIRE(unset.error().code == meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("a dangling sibling link is looked for in the configured sources when outside links are followed", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    held.write("root/leaf.xacro", include_tree::binds("fallback"));
    std::filesystem::create_directories(held.root() / "outside");
    const std::filesystem::path top = held.write("doc/top.xacro", include_tree::reads_leaf("leaf.xacro"));
    if(!include_tree::linked(held.root() / "outside" / "leaf.xacro", held.root() / "doc" / "leaf.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };

    REQUIRE(include_tree::renders_link(expand_with(top, sources, log, following()), "fallback"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("an admitted climb resolves through a linked directory as the operating system does", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    if(!linked_climb_tree(held))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    const std::filesystem::path inner = held.root() / "doc" / "sub" / "inner.xacro";
    const std::string opened = include_tree::read_file(held.root() / "doc" / "sub/../x.xacro");
    const std::string expected = opened.find("through_link") != std::string::npos ? "through_link" : "lexical";
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;

    const include_tree::expansion_result unset = expand_with(inner, sources, log, meios::include_options{});
    REQUIRE_FALSE(unset.has_value());
    REQUIRE(unset.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(include_tree::renders_link(expand_with(inner, sources, log, climbing()), expected));
}

TEST_CASE("a climb back inside a linked directory is resolved by its spelling with the climb admitted or not", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    if(!linked_climb_tree(held))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    const std::filesystem::path top = held.write("doc/top.xacro", include_tree::reads_leaf("sub/../x.xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;

    REQUIRE(include_tree::renders_link(expand_with(top, sources, log, meios::include_options{}), "lexical"));
    REQUIRE(include_tree::renders_link(expand_with(top, sources, log, climbing()), "lexical"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

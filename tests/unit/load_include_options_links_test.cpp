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

bool dangling_tree(const include_tree::tree &held)
{
    held.write("root/leaf.xacro", include_tree::binds("fallback"));
    held.write("root/gone/leaf.xacro", include_tree::binds("fallback"));
    held.write("doc/top.xacro", include_tree::reads_leaf("leaf.xacro"));
    held.write("doc/deep.xacro", include_tree::reads_leaf("gone/leaf.xacro"));
    std::error_code ec;
    std::filesystem::create_directory_symlink(held.root() / "outside" / "gone", held.root() / "doc" / "gone", ec);
    return !ec && include_tree::linked(held.root() / "outside" / "leaf.xacro", held.root() / "doc" / "leaf.xacro");
}

bool refuses_followed(const include_tree::expansion_result &out, const std::string &written, const std::filesystem::path &followed)
{
    return !out.has_value() && out.error().code == meios::diagnostic_code::unresolved_include && out.error().message.starts_with("xacro:include could not resolve \"" + written + '"') &&
            out.error().message.find(followed.string()) != std::string::npos;
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

TEST_CASE("a followed directory link to an absent target refuses even where a configured root holds the name", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    held.write("root/sub/leaf.xacro", include_tree::binds("root_copy"));
    std::filesystem::create_directories(held.root() / "elsewhere");
    const std::filesystem::path top = held.write("doc/top.xacro", include_tree::reads_leaf("sub/leaf.xacro"));
    std::error_code ec;
    std::filesystem::create_directory_symlink(held.root() / "elsewhere", held.root() / "doc" / "sub", ec);
    if(ec)
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };

    REQUIRE(refuses_followed(expand_with(top, sources, log, following()), "sub/leaf.xacro", top.parent_path() / "sub/leaf.xacro"));
    const include_tree::expansion_result unset = expand_with(top, sources, log, meios::include_options{});
    REQUIRE_FALSE(unset.has_value());
    REQUIRE(unset.error().code == meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("a dangling sibling link with outside links followed refuses even where a configured root holds the name", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    if(!dangling_tree(held))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    const std::filesystem::path doc = held.root() / "doc";
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };

    REQUIRE(refuses_followed(expand_with(doc / "top.xacro", sources, log, following()), "leaf.xacro", doc / "leaf.xacro"));
    REQUIRE(refuses_followed(expand_with(doc / "deep.xacro", sources, log, following()), "gone/leaf.xacro", doc / "gone/leaf.xacro"));
}

TEST_CASE("a dangling sibling link with outside links unset falls through to the configured sources", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    if(!dangling_tree(held))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };

    REQUIRE(include_tree::renders_link(expand_with(held.root() / "doc" / "top.xacro", sources, log, meios::include_options{}), "fallback"));
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

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
#include <system_error>
#include <initializer_list>

using include_tree::binds;
using include_tree::reads_leaf;
using include_tree::expand_file;
using include_tree::expand_text;
using include_tree::renders_link;

namespace
{

include_tree::expansion_result expand_under(const std::filesystem::path &document, meios::source_stack &sources, meios::log_sink &log, bool climb, bool follow)
{
    meios::include_options includes;
    includes.climb_above_document = climb;
    includes.follow_outside_links = follow;
    meios::eval_scope scope;
    return meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{}, meios::eval_policy::fail, {}, log, includes);
}

}

TEST_CASE("a relative include beside its document resolves with no configured source", "[xacro][structural][include][resolution]")
{
    const include_tree::tree held;
    held.write("leaf.xacro", binds("beside"));
    const std::filesystem::path top = held.write("top.xacro", reads_leaf("leaf.xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    REQUIRE(renders_link(expand_file(top, sources, log), "beside"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a relative include with nothing beside its document falls back to a configured source", "[xacro][structural][include][resolution]")
{
    const include_tree::tree held;
    held.write("root/leaf.xacro", binds("from_root"));
    const std::filesystem::path top = held.write("doc/top.xacro", reads_leaf("leaf.xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{meios::directory_source{held.root() / "root", log}};

    REQUIRE(renders_link(expand_file(top, sources, log), "from_root"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a document expanded under no name looks for nothing beside it", "[xacro][structural][include][resolution]")
{
    const include_tree::tree held;
    held.write("cwd/leaf.xacro", binds("from_cwd"));
    held.write("root/leaf.xacro", binds("from_root"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{meios::directory_source{held.root() / "root", log}};

    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(held.root() / "cwd");
    const include_tree::expansion_result out = expand_text(reads_leaf("leaf.xacro"), {}, sources, log);
    std::filesystem::current_path(saved);

    REQUIRE(renders_link(out, "from_root"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a relative include found nowhere names the spelling written and both places looked", "[xacro][structural][include][resolution]")
{
    const include_tree::tree held;
    const std::filesystem::path top = held.write("doc/top.xacro", reads_leaf("./leaf.xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{meios::directory_source{held.root() / "root", log}};

    const include_tree::expansion_result out = expand_file(top, sources, log);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::unresolved_include);
    REQUIRE(out.error().message == "xacro:include could not resolve \"./leaf.xacro\": looked for it beside the document that writes it or in the configured sources");
}

TEST_CASE("an absolute include spelling is not looked for beside its document", "[xacro][structural][include][resolution]")
{
    const include_tree::tree held;
    const std::filesystem::path leaf = held.write("leaf.xacro", binds("beside"));
    const std::filesystem::path top  = held.write("top.xacro", reads_leaf(leaf.generic_string()));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    const include_tree::expansion_result out = expand_file(top, sources, log);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::unresolved_include);
    REQUIRE(out.error().message.find("beside") == std::string::npos);
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::unresolved_include) == 1);
}

TEST_CASE("a link beside the document that escapes it refuses without falling through", "[xacro][structural][include][resolution][containment]")
{
    const include_tree::tree held;
    const std::filesystem::path outside = held.write("outside/leaf.xacro", binds("outside"));
    held.write("root/leaf.xacro", binds("from_root"));
    const std::filesystem::path top = held.write("doc/top.xacro", reads_leaf("leaf.xacro"));
    std::error_code ec;
    std::filesystem::create_symlink(outside, held.root() / "doc" / "leaf.xacro", ec);
    if(ec)
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{meios::directory_source{held.root() / "root", log}};

    const include_tree::expansion_result out = expand_file(top, sources, log);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::uncontained_asset);
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::uncontained_asset) == 1);
    REQUIRE(xacro_probe::errors(records) == 1);
}

TEST_CASE("a relative include climbing above its document's directory refuses as escaping", "[xacro][structural][include][resolution]")
{
    const std::filesystem::path top = std::filesystem::path{MEIOS_XACRO_PROBE_DIR} / "include_parent_directory.xacro";
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    const include_tree::expansion_result out = expand_file(top, sources, log);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message.find("climbs above the directory of the document that writes it") != std::string::npos);
    REQUIRE(xacro_probe::errors(records) == 1);
}

TEST_CASE("a document named by a relative path anchors its relative includes at the working directory", "[xacro][structural][include][resolution]")
{
    const include_tree::tree held;
    held.write("cwd/leaf.xacro", binds("from_cwd"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    const include_tree::working_directory here(held.root() / "cwd");
    REQUIRE(renders_link(expand_text(reads_leaf("leaf.xacro"), "top.xacro", sources, log), "from_cwd"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

#ifdef _WIN32
TEST_CASE("a backslash climb is a climb where the backslash separates a path", "[xacro][structural][include][resolution]")
{
    const include_tree::tree held;
    held.write("doc/leaf.xacro", binds("parent"));
    const std::filesystem::path inner = held.write("doc/sub/inner.xacro", reads_leaf("..\\leaf.xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    for(bool follow : { false, true })
    {
        const include_tree::expansion_result refused = expand_under(inner, sources, log, false, follow);
        REQUIRE_FALSE(refused.has_value());
        REQUIRE(refused.error().code == meios::diagnostic_code::xacro_structural_error);
        REQUIRE(refused.error().message.find("climbs above the directory of the document that writes it") != std::string::npos);
    }
    REQUIRE(renders_link(expand_under(inner, sources, log, true, false), "parent"));
}
#else
TEST_CASE("a backslash is part of a file name where only the slash separates a path", "[xacro][structural][include][resolution]")
{
    const include_tree::tree held;
    held.write("doc/..\\leaf.xacro", binds("named"));
    const std::filesystem::path top = held.write("doc/top.xacro", reads_leaf("..\\leaf.xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    REQUIRE(renders_link(expand_under(top, sources, log, false, false), "named"));
    REQUIRE(renders_link(expand_under(top, sources, log, false, true), "named"));
    REQUIRE(xacro_probe::errors(records) == 0);
}
#endif

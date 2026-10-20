#include "xacro_read_probe.h"
#include "xacro_include_tree.h"
#include "load_beside_root_fixture.h"

#include <meios/urdf.h>
#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <filesystem>

using beside_root_fixture::rooted;
using beside_root_fixture::linked_sub;
using beside_root_fixture::loads_link;
using beside_root_fixture::load_outcome;

namespace
{

// doc/common.xacro links into root/common, which neither of the document's directories holds.
bool linked_include(const include_tree::tree &held)
{
    const std::filesystem::path target = held.write("root/common/common.xacro", include_tree::binds("from_root_link"));
    held.write("doc/top.xacro", include_tree::reads_leaf("common.xacro"));
    return include_tree::linked(target, held.root() / "doc" / "common.xacro");
}

include_tree::expansion_result expand_rooted(const std::filesystem::path &document, meios::source_stack &sources, meios::log_sink &log, const std::vector<std::filesystem::path> &roots)
{
    meios::eval_scope scope;
    return meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{}, meios::eval_policy::fail, {}, log, meios::include_options{}, roots);
}

}

TEST_CASE("a beside include linking into the roots an expansion is given is read, and refused with none", "[urdf][xacro][include][link]")
{
    const include_tree::tree held;
    if(!linked_include(held))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    const std::filesystem::path top = held.root() / "doc" / "top.xacro";
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{meios::directory_source{held.root() / "root", log}};

    REQUIRE(include_tree::renders_link(expand_rooted(top, sources, log, {held.root() / "root"}), "from_root_link"));
    REQUIRE(xacro_probe::errors(records) == 0);
    const include_tree::expansion_result unrooted = expand_rooted(top, sources, log, {});
    REQUIRE_FALSE(unrooted.has_value());
    REQUIRE(unrooted.error().code == meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("an include linked beside its description into a configured root loads", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    if(!linked_include(held))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome loaded = meios::load(held.root() / "doc" / "top.xacro", rooted(held, false));
    if(!loaded)
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
    REQUIRE(loads_link(loaded, "from_root_link"));
}

TEST_CASE("an include spelled back through a linked directory into a configured root reads where the link leads, and its nested include anchors there", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    held.write("root/common/x.xacro", include_tree::robot(include_tree::include("y.xacro")));
    held.write("root/common/y.xacro", include_tree::binds("nested_through_link"));
    held.write("doc/y.xacro", include_tree::binds("nested_lexical"));
    const std::filesystem::path document = held.write("doc/robot.xacro", include_tree::reads_leaf("sub/../x.xacro"));
    if(!linked_sub(held, "root/common"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    const load_outcome loaded = meios::load(document, rooted(held, false));
    if(!loaded)
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
    REQUIRE(loads_link(loaded, "nested_through_link"));
}

#include "xacro_include_tree.h"

#include <meios/urdf.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <filesystem>
#include <string_view>

namespace
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

meios::load_options under(std::vector<std::filesystem::path> roots)
{
    meios::load_options opts;
    opts.package_roots = std::move(roots);
    return opts;
}

std::string only_link(const load_outcome &loaded)
{
    if(!loaded)
    {
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
        return {};
    }
    return loaded->robot.links.size() == 1 ? loaded->robot.links.front().name : std::string{};
}

}

TEST_CASE("a sibling miss spelled back through a linked directory is not asked of the serving layer, so it refuses as the reference refuses it", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    held.write("R/p/urdf/top.xacro", include_tree::robot(include_tree::include("sub/../c.xacro")));
    held.write("R/p/urdf/c.xacro", include_tree::binds("lexical_c"));
    std::filesystem::create_directories(held.root() / "R/p/urdf/a/b");
    if(!include_tree::linked("a/b", held.root() / "R/p/urdf/sub"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    for(const std::string_view spelling : {"$(find p)/urdf/top.xacro", "package://p/urdf/top.xacro"})
        DYNAMIC_SECTION(spelling)
        {
            const load_outcome loaded = meios::load(held.write("T/top.xacro", include_tree::reads_leaf(spelling)), under({held.root() / "R"}));
            CHECK(only_link(loaded).empty());
            REQUIRE_FALSE(loaded.has_value());
            INFO(loaded.error().message);
            CHECK(loaded.error().code == meios::diagnostic_code::unresolved_include);
            CHECK(loaded.error().message.find("\"sub/../c.xacro\"") != std::string::npos);
        }
}

TEST_CASE("a byte-served document's sibling spelled through a parent element is still asked of the layer that served it", "[urdf][load][include][source]")
{
    const include_tree::tree held;
    meios::log_sink silent;
    meios::capturing_log_sink log{silent};
    meios::memory_source bytes{log};
    bytes.add("p", "urdf/top.xacro", include_tree::robot(include_tree::include("sub/../c.xacro")));
    bytes.add("p", "urdf/c.xacro", include_tree::binds("bytes_c"));
    meios::source_stack sources{std::move(bytes)};

    const std::filesystem::path top = held.write("T/top.xacro", include_tree::reads_leaf("$(find p)/urdf/top.xacro"));
    CHECK(only_link(meios::load(top, {}, sources, log)) == "bytes_c");
}

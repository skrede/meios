#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_including(const std::string &filename)
{
    meios::log_sink_f log{ [](auto &&...) {} };
    const include_tree::tree held;
    held.write("root/p/urdf/c.xacro", include_tree::robot("<link name=\"p_urdf_c\"/>"));
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };
    const std::string text = include_tree::robot("<xacro:property name=\"sp\" value=\" p\"/>" + include_tree::include(filename));
    return include_tree::expand_text(text, held.root() / "doc" / "top.xacro", sources, log);
}

}

TEST_CASE("a padded or substituted find package renders its package's file", "[xacro][include][spelling]")
{
    for(const char *filename : { "$(find  p)/urdf/c.xacro", "$(find p )/urdf/c.xacro", "$(find ${sp})/urdf/c.xacro" })
    {
        INFO("filename: " << filename);
        CHECK(include_tree::renders_link(expand_including(filename), "p_urdf_c"));
    }
}

TEST_CASE("a find composed with text that is not a separator does not reach its package", "[xacro][include][spelling]")
{
    const expansion_result out = expand_including("$(find p)urdf/c.xacro");
    CHECK_FALSE(include_tree::renders_link(out, "p_urdf_c"));
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::unresolved_include);
}

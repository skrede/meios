#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_body(const std::string &body)
{
    meios::log_sink_f log{ [](auto &&...) {} };
    const include_tree::tree held;
    held.write("root/p/urdf/c.xacro", include_tree::robot("<link name=\"p_urdf_c\"/>"));
    held.write("root/p/c.xacro", include_tree::robot("<link name=\"p_c\"/>"));
    held.write("root/q/a.xacro", include_tree::robot("<link name=\"q_a\"/>"));
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };
    const std::string text = include_tree::robot("<xacro:property name=\"sp\" value=\" p\"/>" + body);
    return include_tree::expand_text(text, held.root() / "doc" / "top.xacro", sources, log);
}

expansion_result expand_including(const std::string &filename)
{
    return expand_body(include_tree::include(filename));
}

void require_refused(const expansion_result &out, meios::diagnostic_code code, const std::string &names)
{
    REQUIRE_FALSE(out.has_value());
    INFO(out.error().message);
    CHECK(out.error().code == code);
    CHECK(out.error().message.find(names) != std::string::npos);
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

TEST_CASE("a path under a find's directory renders its package's file however the path is written", "[xacro][include][spelling]")
{
    for(const char *filename : { "$(find p)//urdf/c.xacro", "$(find p)/./urdf/c.xacro", "${''}$(find p)/urdf/c.xacro", "${'$(find p)'}/urdf/c.xacro" })
    {
        INFO("filename: " << filename);
        CHECK(include_tree::renders_link(expand_including(filename), "p_urdf_c"));
    }
}

TEST_CASE("a find composed with text that is not a separator does not reach its package", "[xacro][include][spelling]")
{
    for(const char *filename : { "$(find p)urdf/c.xacro", "$(find p)./urdf/c.xacro", "$(find p)urdf/../urdf/c.xacro", "x/$(find p)/urdf/c.xacro" })
    {
        INFO("filename: " << filename);
        const expansion_result out = expand_including(filename);
        CHECK_FALSE(include_tree::renders_link(out, "p_urdf_c"));
        require_refused(out, meios::diagnostic_code::unresolved_include, "could not resolve");
    }
}

TEST_CASE("a find given a second word refuses naming it", "[xacro][include][spelling]")
{
    require_refused(expand_including("$(find p q)/urdf/c.xacro"), meios::diagnostic_code::unresolved_find, "\"q\"");
    require_refused(expand_body("<link name=\"$(find p urdf)\"/>"), meios::diagnostic_code::unresolved_find, "\"urdf\"");
}

TEST_CASE("a path that climbs out of a find's package refuses", "[xacro][include][spelling]")
{
    const expansion_result out = expand_including("$(find p)/../q/a.xacro");
    CHECK_FALSE(include_tree::renders_link(out, "q_a"));
    require_refused(out, meios::diagnostic_code::xacro_structural_error, "escapes the source root");
}

TEST_CASE("an include filename refuses at its first refusing token, at an offset into the whole filename", "[xacro][include][spelling]")
{
    const std::pair<const char *, const char *> unterminated[] = {
        { "$(find ${nope})${x", "at offset 15" },
        { "$(find p)/urdf/${x", "at offset 15" },
    };
    for(const auto &[filename, names] : unterminated)
    {
        INFO("filename: " << filename);
        require_refused(expand_including(filename), meios::diagnostic_code::unterminated_substitution, names);
    }
    require_refused(expand_including("$(find nopkg)/${nope}.xacro"), meios::diagnostic_code::unresolved_find, "$(find nopkg)");
    require_refused(expand_including("$(find ${nope})$$x"), meios::diagnostic_code::expression_error, "invalid expression: $$x");
}

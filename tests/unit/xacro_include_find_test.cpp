#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

const std::string head = "<robot name=\"p\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
                         "<xacro:property name=\"pk\" value=\"p\"/><xacro:property name=\"e\" value=\"\"/>\n";

expansion_result expand_over(meios::source_stack &sources, const include_tree::tree &held, const std::string &filename, meios::log_sink &log)
{
    return include_tree::expand_text(head + include_tree::include(filename) + "</robot>", held.root() / "doc" / "top.xacro", sources, log);
}

expansion_result expand_including(const std::string &filename)
{
    meios::log_sink_f log{ [](auto &&...) {} };
    const include_tree::tree held;
    held.write("root/p/urdf/a.xacro", include_tree::robot("<link name=\"from_package\"/>"));
    held.write("doc/a.xacro", include_tree::robot("<link name=\"beside_a\"/>"));
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };
    return expand_over(sources, held, filename, log);
}

expansion_result expand_from_memory(const std::string &filename)
{
    meios::log_sink_f log{ [](auto &&...) {} };
    const include_tree::tree held;
    meios::memory_source memory{ log };
    memory.add("p", "urdf/a.xacro", include_tree::robot("<link name=\"from_package\"/>"));
    meios::source_stack sources{ std::move(memory) };
    return expand_over(sources, held, filename, log);
}

void require_refused(const expansion_result &out, meios::diagnostic_code code, const std::string &message)
{
    REQUIRE_FALSE(out.has_value());
    INFO(out.error().message);
    CHECK(out.error().code == code);
    CHECK(out.error().message.starts_with(message));
}

}

TEST_CASE("a token after an include's leading find that does not lex refuses before the package text is evaluated", "[xacro][include][find]")
{
    const std::pair<const char *, const char *> unlexed[] = {
        { "$(find ${nope})$$x", "invalid expression: $$x" },
        { "$(find nopkg)$$x", "invalid expression: $$x" },
    };
    for(const auto &[filename, message] : unlexed)
    {
        INFO("filename: " << filename);
        require_refused(expand_including(filename), meios::diagnostic_code::expression_error, message);
    }
    const std::pair<const char *, const char *> unterminated[] = {
        { "$(find ${nope})${x", "unterminated ${ substitution span at offset 15" },
        { "$(find ${nope})$(x", "unterminated $( substitution span at offset 15" },
    };
    for(const auto &[filename, message] : unterminated)
    {
        INFO("filename: " << filename);
        require_refused(expand_including(filename), meios::diagnostic_code::unterminated_substitution, message);
    }
}

TEST_CASE("a token after the find that lexes leaves the package text's own cause", "[xacro][include][find]")
{
    for(const char *filename : { "$(find ${nope})/a.xacro", "$(find ${nope})a$$x", "$(find ${nope})$x", "$(find ${nope})$${x}" })
    {
        INFO("filename: " << filename);
        require_refused(expand_including(filename), meios::diagnostic_code::undefined_property, "name 'nope' is not defined");
    }
}

TEST_CASE("the lookahead refusal is located at the include element", "[xacro][include][find]")
{
    const expansion_result out = expand_including("$(find ${nope})$$x");
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().loc.line == 3);
}

TEST_CASE("the package of a leading find is resolved before the rest of the filename is substituted", "[xacro][include][find]")
{
    for(const char *filename : { "$(find nopkg)/${nope}.xacro", "$(find nopkg)/a.xacro" })
    {
        INFO("filename: " << filename);
        require_refused(expand_including(filename), meios::diagnostic_code::unresolved_find, "$(find nopkg) did not resolve");
    }
    require_refused(expand_including("$(find p)/${nope}.xacro"), meios::diagnostic_code::undefined_property, "name 'nope' is not defined");
    CHECK(include_tree::renders_link(expand_including("$(find p)/urdf/a.xacro"), "from_package"));
}

TEST_CASE("a find package read from a bound property renders its package's file", "[xacro][include][find]")
{
    CHECK(include_tree::renders_link(expand_including("$(find ${pk})/urdf/a.xacro"), "from_package"));
}

TEST_CASE("a find with no package refuses as the find command refuses it", "[xacro][include][find]")
{
    for(const char *filename : { "$(find )/a.xacro", "$(find ${e})/a.xacro" })
    {
        INFO("filename: " << filename);
        const expansion_result out = expand_including(filename);
        CHECK_FALSE(include_tree::renders_link(out, "beside_a"));
        require_refused(out, meios::diagnostic_code::unresolved_find, "$(find) requires a package name");
    }
}

TEST_CASE("a layer that serves only bytes is not asked for a bare package", "[xacro][include][find]")
{
    CHECK(include_tree::renders_link(expand_from_memory("$(find p)/urdf/a.xacro"), "from_package"));
    const expansion_result missing = expand_from_memory("$(find nopkg)/a.xacro");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code == meios::diagnostic_code::unresolved_include);
}

TEST_CASE("a find naming its package by a rooted path names no package, even a package directory inside a configured root", "[xacro][include][find]")
{
    meios::log_sink_f log{ [](auto &&...) {} };
    const include_tree::tree held;
    held.write("root/p/urdf/a.xacro", include_tree::robot("<link name=\"from_package\"/>"));
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };
    const std::string named   = (held.root() / "root" / "p").generic_string();
    const std::string refusal = "$(find " + named + ") did not resolve";
    require_refused(expand_over(sources, held, "$(find " + named + ")/urdf/a.xacro", log), meios::diagnostic_code::unresolved_find, refusal);
    const std::string attribute = head + "<link name=\"$(find " + named + ")\"/></robot>";
    require_refused(include_tree::expand_text(attribute, held.root() / "doc" / "top.xacro", sources, log), meios::diagnostic_code::unresolved_find, refusal);
    CHECK(include_tree::renders_link(expand_over(sources, held, "$(find p)/urdf/a.xacro", log), "from_package"));
}

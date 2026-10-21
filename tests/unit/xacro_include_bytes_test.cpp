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

meios::memory_source memory_holding_a(meios::log_sink &log)
{
    meios::memory_source memory{ log };
    memory.add("p", "urdf/a.xacro", include_tree::robot("<link name=\"memory_a\"/>"));
    return memory;
}

expansion_result expand_over(meios::source_stack &sources, const include_tree::tree &held, const std::string &filename, meios::log_sink &log)
{
    return include_tree::expand_text(include_tree::robot("\n" + include_tree::include(filename)), held.root() / "doc" / "top.xacro", sources, log);
}

expansion_result expand_from_memory(const std::string &filename)
{
    meios::log_sink_f log{ [](auto &&...) {} };
    const include_tree::tree held;
    meios::source_stack sources{ memory_holding_a(log) };
    return expand_over(sources, held, filename, log);
}

expansion_result expand_from_memory_over_directory(const std::string &filename)
{
    meios::log_sink_f log{ [](auto &&...) {} };
    const include_tree::tree held;
    held.write("root/p/urdf/a.xacro", include_tree::robot("<link name=\"directory_a\"/>"));
    meios::source_stack sources{ memory_holding_a(log), meios::directory_source{ held.root() / "root", log } };
    return expand_over(sources, held, filename, log);
}

}

TEST_CASE("a memory layer still serves a find's package as the filename's first token", "[xacro][include][bytes]")
{
    for(const char *filename : { "$(find p)/urdf/a.xacro", "$(find  p)/urdf/a.xacro" })
    {
        INFO("filename: " << filename);
        CHECK(include_tree::renders_link(expand_from_memory(filename), "memory_a"));
    }
}

TEST_CASE("a memory layer's find composed with text or nested in a span refuses naming the package", "[xacro][include][bytes]")
{
    for(const char *filename : { "$(find p)urdf/a.xacro", "x$(find p)/urdf/a.xacro", "${'$(find p)'}/urdf/a.xacro" })
    {
        INFO("filename: " << filename);
        const expansion_result out = expand_from_memory(filename);
        REQUIRE_FALSE(out.has_value());
        INFO(out.error().message);
        CHECK(out.error().code == meios::diagnostic_code::unresolved_include);
        CHECK(out.error().message.find("$(find p) names no directory") != std::string::npos);
        CHECK(out.error().loc.line == 2);
    }
}

TEST_CASE("a memory layer's find of a package it does not hold refuses as an unresolved include", "[xacro][include][bytes]")
{
    const expansion_result out = expand_from_memory("$(find nopkg)/a.xacro");
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::unresolved_include);
}

TEST_CASE("a memory layer above a directory layer outranks it for a find's package file", "[xacro][include][bytes]")
{
    const expansion_result out = expand_from_memory_over_directory("$(find p)/urdf/a.xacro");
    CHECK(include_tree::renders_link(out, "memory_a"));
    CHECK_FALSE(include_tree::renders_link(out, "directory_a"));
}

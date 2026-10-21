#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_from_memory(const std::string &filename)
{
    meios::log_sink_f log{ [](auto &&...) {} };
    const include_tree::tree held;
    meios::memory_source memory{ log };
    memory.add("p", "urdf/a.xacro", include_tree::robot("<link name=\"memory_a\"/>"));
    meios::source_stack sources{ std::move(memory) };
    return include_tree::expand_text(include_tree::robot(include_tree::include(filename)), held.root() / "doc" / "top.xacro", sources, log);
}

}

TEST_CASE("a memory layer still serves a find's package as the filename's first token", "[xacro][include][bytes]")
{
    CHECK(include_tree::renders_link(expand_from_memory("$(find p)/urdf/a.xacro"), "memory_a"));
}

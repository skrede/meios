#include "error_sink.h"
#include "xacro_include_tree.h"

#include <meios/io/source_stack.h>
#include <meios/io/bundle_source.h>
#include <meios/io/memory_source.h>
#include <meios/io/source_handle.h>
#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>
#include <meios/io/directory_source.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace
{

struct locate_only
{
    std::reference_wrapper<std::int32_t> located;

    meios::capability_descriptor capabilities() const
    {
        return {meios::source_kind::memory, false};
    }

    std::optional<meios::resolved_asset> locate(std::string_view, std::string_view)
    {
        ++located.get();
        return meios::resolved_asset{std::filesystem::path{"found"}};
    }
};

}

TEST_CASE("a layer without a quiet answer is asked through locate for the shadow note", "[io][stack]")
{
    std::int32_t above = 0;
    std::int32_t below = 0;
    error_sink_test::error_sink heard;
    meios::source_stack stack{locate_only{above}, locate_only{below}};

    REQUIRE(stack.locate("pkg", "file", heard).has_value());
    REQUIRE(stack.try_locate("pkg", "file", heard).value().has_value());
    REQUIRE(above == 2);
    REQUIRE(below == 2);
}

TEST_CASE("a path-backed layer answers whether it would serve a file without logging", "[io][stack]")
{
    const include_tree::tree held;
    const std::filesystem::path root    = held.root() / "root";
    const std::filesystem::path outside = held.write("outside.txt", "out");
    held.write("root/pkg/in.txt", "in");
    if(!include_tree::linked(outside, root / "pkg" / "out.txt"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    error_sink_test::error_sink heard;
    const auto quiet = [](meios::source_handle layer)
    {
        CHECK(layer.answers("pkg", "in.txt"));
        CHECK_FALSE(layer.answers("pkg", "out.txt"));
        CHECK_FALSE(layer.answers("pkg", "missing.txt"));
    };
    for(const bool bundle : {false, true})
        DYNAMIC_SECTION((bundle ? "bundle" : "directory"))
        {
            quiet(bundle ? meios::source_handle{meios::bundle_source{root, heard}} : meios::source_handle{meios::directory_source{root, heard}});
            for(const error_sink_test::error_record &one : heard.errors)
                UNSCOPED_INFO(one.message);
            CHECK(heard.errors.empty());
        }
}

TEST_CASE("a memory layer asked whether it would answer writes nothing to its mirror", "[io][stack]")
{
    const include_tree::tree held;
    held.write("pkg/b.txt", "high");
    error_sink_test::error_sink heard;
    meios::memory_source memory{heard};
    memory.add("pkg", "a.txt", "a").add("pkg", "b.txt", "b");
    meios::source_stack stack{meios::directory_source{held.root(), heard}, std::move(memory)};
    meios::source_handle &layer                  = stack.layers()[1];
    const std::optional<meios::resolved_asset> a = layer.locate("pkg", "a.txt");
    REQUIRE(a.has_value());
    const std::filesystem::path b = a->path().parent_path() / "b.txt";

    REQUIRE(layer.answers("pkg", "b.txt"));
    CHECK_FALSE(std::filesystem::exists(b));
    REQUIRE(stack.locate("pkg", "b.txt", heard).has_value());
    CHECK_FALSE(std::filesystem::exists(b));
    REQUIRE(layer.locate("pkg", "b.txt").has_value());
    CHECK(std::filesystem::exists(b));
    CHECK(heard.errors.empty());
}

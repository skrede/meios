#ifndef HPP_GUARD_MEIOS_UNIT_LOAD_SERVED_LAYER_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_LOAD_SERVED_LAYER_FIXTURE_H

#include "xacro_include_tree.h"

#include <meios/urdf.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <filesystem>
#include <string_view>

namespace served_layer_fixture
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

inline meios::load_options under(std::vector<std::filesystem::path> roots, bool climbing = false)
{
    meios::load_options opts;
    opts.package_roots                 = std::move(roots);
    opts.includes.climb_above_document = climbing;
    return opts;
}

inline std::string only_link(const load_outcome &loaded)
{
    if(!loaded)
    {
        UNSCOPED_INFO(static_cast<int>(loaded.error().code) << ": " << loaded.error().message);
        return {};
    }
    return loaded->robot.links.size() == 1 ? loaded->robot.links.front().name : std::string{};
}

inline void refuses_unresolved(const load_outcome &loaded, const std::string &written)
{
    CHECK(only_link(loaded).empty());
    REQUIRE_FALSE(loaded.has_value());
    INFO(loaded.error().message);
    CHECK(loaded.error().code == meios::diagnostic_code::unresolved_include);
    CHECK(loaded.error().message.find('"' + written + '"') != std::string::npos);
}

inline std::filesystem::path top_reading_package(const include_tree::tree &held, std::string_view spelling = "$(find p)/urdf/a.xacro")
{
    return held.write("T/top.xacro", include_tree::reads_leaf(spelling));
}

// R/p/urdf/sub links to R/e/d, so a.xacro's sub/../b.xacro names R/e/b.xacro where the link is followed before .. is reduced, and R/p/urdf/b.xacro where .. is reduced first.
inline bool beside_hit_through_link(const include_tree::tree &held)
{
    held.write("R/p/urdf/a.xacro", include_tree::robot(include_tree::include("sub/../b.xacro")));
    held.write("R/e/b.xacro", include_tree::robot(include_tree::include("c.xacro")));
    held.write("R/p/urdf/c.xacro", include_tree::binds("layer_sibling"));
    std::filesystem::create_directories(held.root() / "R/e/d");
    return include_tree::linked(held.root() / "R/e/d", held.root() / "R/p/urdf/sub");
}

}

#endif

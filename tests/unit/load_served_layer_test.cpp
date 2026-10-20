#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/urdf.h>

#include <meios/io/bundle_source.h>
#include <meios/io/memory_source.h>
#include <meios/io/source_handle.h>
#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

struct bytes_only
{
    static meios::capability_descriptor capabilities()
    {
        return {meios::source_kind::memory, false};
    }

    std::optional<meios::resolved_asset> locate(std::string_view, std::string_view)
    {
        return std::nullopt;
    }
};

meios::load_options climbing_under(std::vector<std::filesystem::path> roots)
{
    meios::load_options opts;
    opts.package_roots                 = std::move(roots);
    opts.includes.climb_above_document = true;
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

void refuses_unresolved(const load_outcome &loaded, const std::string &written)
{
    CHECK(only_link(loaded).empty());
    REQUIRE_FALSE(loaded.has_value());
    INFO(loaded.error().message);
    CHECK(loaded.error().code == meios::diagnostic_code::unresolved_include);
    CHECK(loaded.error().message.find('"' + written + '"') != std::string::npos);
}

std::filesystem::path top_reading_package(const include_tree::tree &held)
{
    return held.write("T/top.xacro", include_tree::reads_leaf("$(find p)/urdf/a.xacro"));
}

}

TEST_CASE("a layer's type alone says whether it names paths", "[urdf][load][include][source]")
{
    const include_tree::tree held;
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};

    CHECK(meios::source_handle(meios::directory_source{held.root(), log}).names_paths());
    CHECK(meios::source_handle(meios::bundle_source{held.root(), log}).names_paths());
    CHECK_FALSE(meios::source_handle(meios::memory_source{log}).names_paths());
    CHECK_FALSE(meios::source_handle(bytes_only{}).names_paths());
    CHECK(records.empty());
}

TEST_CASE("a beside link into a second configured root climbs as the reference reads it, with no record naming a path the description never wrote", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    held.write("R1/p/urdf/a.xacro", include_tree::robot(include_tree::include("common.xacro")));
    const std::filesystem::path shared = held.write("R2/common/common.xacro", include_tree::robot(include_tree::include("../x.xacro")));
    held.write("R1/p/x.xacro", include_tree::binds("via_r1"));
    if(!include_tree::linked(shared, held.root() / "R1/p/urdf/common.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    const load_outcome loaded = meios::load(top_reading_package(held), climbing_under({held.root() / "R1", held.root() / "R2"}), log);
    CHECK(only_link(loaded) == "via_r1");
    CHECK(xacro_probe::errors(records) == 0);
}

TEST_CASE("a sibling of a document served through an in-root link refuses where nothing is beside its named path", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    const std::filesystem::path served = held.write("R/q/a.xacro", include_tree::robot(include_tree::include("b.xacro")));
    held.write("R/q/b.xacro", include_tree::robot(include_tree::include("../c.xacro")));
    held.write("R/c.xacro", include_tree::binds("disk_route_R_c"));
    held.write("R/p/c.xacro", include_tree::binds("layer_route_R_p_c"));
    std::filesystem::create_directories(held.root() / "R/p/urdf");
    if(!include_tree::linked(served, held.root() / "R/p/urdf/a.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    refuses_unresolved(meios::load(top_reading_package(held), climbing_under({held.root() / "R"})), "b.xacro");
}

TEST_CASE("a beside hit a configured root admitted carries no served record, so the serving layer does not answer its miss", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    held.write("R/p/urdf/a.xacro", include_tree::robot(include_tree::include("sub/../b.xacro")));
    held.write("R/e/b.xacro", include_tree::robot(include_tree::include("c.xacro")));
    held.write("R/p/urdf/c.xacro", include_tree::binds("layer_sibling"));
    std::filesystem::create_directories(held.root() / "R/e/d");
    if(!include_tree::linked(held.root() / "R/e/d", held.root() / "R/p/urdf/sub"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    meios::load_options opts;
    opts.package_roots = {held.root() / "R"};
    refuses_unresolved(meios::load(top_reading_package(held), opts), "c.xacro");
}

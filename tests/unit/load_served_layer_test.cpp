#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/urdf.h>

#include <meios/io/source_stack.h>
#include <meios/io/bundle_source.h>
#include <meios/io/memory_source.h>
#include <meios/io/source_handle.h>
#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/capturing_log_sink.h>

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

// Serves a package file from a second tree where the first lacks it, as an overlaid workspace does.
struct overlaid
{
    std::vector<std::filesystem::path> roots;

    static meios::capability_descriptor capabilities()
    {
        return {meios::source_kind::directory, false};
    }

    std::optional<std::filesystem::path> path_of(std::string_view package, std::string_view relative) const
    {
        for(const std::filesystem::path &root : roots)
            if(std::filesystem::exists(root / package / relative))
                return root / package / relative;
        return std::nullopt;
    }

    std::optional<meios::resolved_asset> locate(std::string_view package, std::string_view relative)
    {
        if(const std::optional<std::filesystem::path> found = path_of(package, relative))
            return meios::resolved_asset{*found, found->parent_path(), found->filename(), *found};
        return std::nullopt;
    }
};

meios::load_options under(std::vector<std::filesystem::path> roots, bool climbing = false)
{
    meios::load_options opts;
    opts.package_roots                 = std::move(roots);
    opts.includes.climb_above_document = climbing;
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

std::filesystem::path top_reading_package(const include_tree::tree &held, std::string_view spelling = "$(find p)/urdf/a.xacro")
{
    return held.write("T/top.xacro", include_tree::reads_leaf(spelling));
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
    const load_outcome loaded = meios::load(top_reading_package(held), under({held.root() / "R1", held.root() / "R2"}, true), log);
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

    refuses_unresolved(meios::load(top_reading_package(held), under({held.root() / "R"}, true)), "b.xacro");
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

    refuses_unresolved(meios::load(top_reading_package(held), under({held.root() / "R"})), "c.xacro");
}

TEST_CASE("a sibling spelled back through a linked directory carries no served record, so its miss refuses as the reference refuses it", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    held.write("R/p/urdf/top.xacro", include_tree::robot(include_tree::include("sub/../b.xacro")));
    held.write("R/p/urdf/a/b.xacro", include_tree::robot(include_tree::include("c.xacro")));
    held.write("R/p/urdf/b.xacro", include_tree::binds("lexical_b"));
    held.write("R/p/urdf/c.xacro", include_tree::binds("layer_lexical_c"));
    std::filesystem::create_directories(held.root() / "R/p/urdf/a/b");
    if(!include_tree::linked("a/b", held.root() / "R/p/urdf/sub"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    refuses_unresolved(meios::load(top_reading_package(held, "$(find p)/urdf/top.xacro"), under({held.root() / "R"})), "c.xacro");
    refuses_unresolved(meios::load(top_reading_package(held, "package://p/urdf/top.xacro"), under({held.root() / "R"})), "c.xacro");
}

TEST_CASE("a sibling spelled without a parent element keeps the served record, so the serving layer answers its miss", "[urdf][load][include][source]")
{
    const include_tree::tree held;
    held.write("R/p/urdf/top.xacro", include_tree::robot(include_tree::include("a/b.xacro")));
    held.write("R/p/urdf/a/b.xacro", include_tree::robot(include_tree::include("c.xacro")));
    held.write("O/p/urdf/a/c.xacro", include_tree::binds("overlay_c"));

    meios::log_sink silent;
    meios::capturing_log_sink log{silent};
    meios::source_stack sources{overlaid{{held.root() / "R", held.root() / "O"}}};
    CHECK(only_link(meios::load(top_reading_package(held, "$(find p)/urdf/top.xacro"), {}, sources, log)) == "overlay_c");
}

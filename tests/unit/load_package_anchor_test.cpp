#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/urdf.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>
#include <string_view>

namespace
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

meios::load_options rooted_at(const std::filesystem::path &root)
{
    meios::load_options opts;
    opts.package_roots = {root};
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

// R/p/urdf/a.xacro is a link to R/q/a.xacro; a case writes a different copy of a sibling into each
// directory, so the rendered name says which one was read.
bool linked_package(const include_tree::tree &held, std::string_view served)
{
    const std::filesystem::path target = held.write("R/q/a.xacro", served);
    std::filesystem::create_directories(held.root() / "R/p/urdf");
    return include_tree::linked(target, held.root() / "R/p/urdf/a.xacro");
}

std::string includes_only(std::string_view spelling)
{
    return include_tree::robot(include_tree::include(spelling));
}

void reads_named(const include_tree::tree &held, std::string (*top)(std::string_view), const std::string &expected)
{
    for(const std::string_view spelling : {"$(find p)/urdf/a.xacro", "package://p/urdf/a.xacro"})
    {
        INFO(spelling);
        const std::filesystem::path document = held.write("doc/robot.xacro", top(spelling));
        std::vector<xacro_probe::captured> records;
        meios::log_sink_f log{xacro_probe::recorder{records}};
        CHECK(only_link(meios::load(document, rooted_at(held.root() / "R"), log)) == expected);
        CHECK(xacro_probe::errors(records) == 0);
    }
}

}

TEST_CASE("a document a package lookup served through an in-root link anchors its includes where it was named", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    held.write("R/p/urdf/b.xacro", include_tree::binds("named_dir_b"));
    held.write("R/q/b.xacro", include_tree::binds("canonical_dir_b"));
    if(!linked_package(held, includes_only("b.xacro")))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    reads_named(held, include_tree::reads_leaf, "named_dir_b");
}

#ifdef MEIOS_HAS_YAML

TEST_CASE("a document a package lookup served through an in-root link anchors its auxiliary documents where it was named", "[urdf][load][yaml][link]")
{
    const include_tree::tree held;
    held.write("R/p/urdf/c.yaml", "name: named_dir_yaml\n");
    held.write("R/q/c.yaml", "name: canonical_dir_yaml\n");
    if(!linked_package(held, include_tree::robot("<link name=\"${xacro.load_yaml('c.yaml')['name']}\"/>")))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    reads_named(held, includes_only, "named_dir_yaml");
}

#endif

TEST_CASE("a package-served document reached through an in-root link is keyed by its canonical file", "[urdf][load][include][link]")
{
    const include_tree::tree held;
    if(!linked_package(held, includes_only("$(find p)/urdf/a.xacro")))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    const std::filesystem::path document = held.write("doc/robot.xacro", includes_only("$(find p)/urdf/a.xacro"));

    const load_outcome refused = meios::load(document, rooted_at(held.root() / "R"));
    REQUIRE_FALSE(refused.has_value());
    INFO(refused.error().message);
    REQUIRE(refused.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(refused.error().message.find(std::filesystem::weakly_canonical(held.root() / "R" / "q" / "a.xacro").string()) != std::string::npos);
}

#include <meios/ros/ros_package_source.h>

#include <meios/io/source_stack.h>
#include <meios/io/package_source.h>
#include <meios/io/directory_source.h>
#include <meios/io/resolved_asset.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <utility>
#include <iterator>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

static_assert(meios::package_source<meios::ros_package_source>);
static_assert(meios::provides_path<meios::ros_package_source>);
static_assert(meios::enumerates_packages<meios::ros_package_source>);

namespace
{

struct capture
{
    std::vector<std::pair<meios::level, std::string>> &entries;

    void operator()(meios::level lvl, const std::string &message) { entries.push_back({ lvl, message }); }

    void operator()(meios::level, const meios::source_location &, const std::string &) {}
};

struct temp_tree
{
    std::filesystem::path root;

    temp_tree()
    {
        static std::atomic<int> counter{ 0 };
        root = std::filesystem::temp_directory_path()
             / ("meios_ros_" + std::to_string(counter.fetch_add(1)) + '_' + std::to_string(::rand()));
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
    }

    ~temp_tree()
    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
};

void write_file(const std::filesystem::path &path, std::string_view content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path);
    stream << content;
}

void make_ament_package(const std::filesystem::path &prefix, std::string_view pkg)
{
    write_file(prefix / "share" / "ament_index" / "resource_index" / "packages" / pkg, "");
    write_file(prefix / "share" / pkg / "meshes" / "x.stl", "solid\n");
}

bool has_package(const std::vector<std::string> &names, std::string_view wanted)
{
    return std::find(names.begin(), names.end(), std::string(wanted)) != names.end();
}

}

TEST_CASE("a ROS2 ament prefix resolves package://arm/meshes and reports capabilities", "[ros]")
{
    temp_tree tree;
    make_ament_package(tree.root, "arm");

    std::vector<std::pair<meios::level, std::string>> log_entries;
    meios::log_sink_f log{ capture{ log_entries } };
    meios::ros_package_source source({}, { tree.root }, log);

    const meios::capability_descriptor caps = source.capabilities();
    REQUIRE(caps.kind == meios::source_kind::directory);
    REQUIRE(caps.path_backed);
    REQUIRE(caps.package_enumerable);

    std::optional<meios::resolved_asset> asset = source.locate("arm", "meshes/x.stl");
    REQUIRE(asset.has_value());
    REQUIRE(asset->holds_path());
    REQUIRE(std::filesystem::exists(asset->path()));
    REQUIRE(has_package(source.packages(), "arm"));
}

TEST_CASE("a relative that escapes the resolved share dir is rejected", "[ros]")
{
    temp_tree tree;
    make_ament_package(tree.root, "arm");

    std::vector<std::pair<meios::level, std::string>> log_entries;
    meios::log_sink_f log{ capture{ log_entries } };
    meios::ros_package_source source({}, { tree.root }, log);

    REQUIRE_FALSE(source.locate("arm", "../../../etc/passwd").has_value());
    const bool rejected = std::any_of(log_entries.begin(), log_entries.end(),
        [](const auto &e) { return e.first == meios::level::error; });
    REQUIRE(rejected);
}

TEST_CASE("ROS1 crawl names packages from the manifest and skips CATKIN_IGNORE", "[ros]")
{
    temp_tree tree;
    write_file(tree.root / "gripper" / "package.xml",
        "<package><name>gripper_pkg</name></package>");
    write_file(tree.root / "gripper" / "meshes" / "g.stl", "solid\n");
    write_file(tree.root / "hidden" / "package.xml",
        "<package><name>hidden_pkg</name></package>");
    write_file(tree.root / "hidden" / "CATKIN_IGNORE", "");

    std::vector<std::pair<meios::level, std::string>> log_entries;
    meios::log_sink_f log{ capture{ log_entries } };
    meios::ros_package_source source({ tree.root }, {}, log);

    const std::vector<std::string> names = source.packages();
    REQUIRE(has_package(names, "gripper_pkg"));
    REQUIRE_FALSE(has_package(names, "hidden_pkg"));
    REQUIRE(source.locate("gripper_pkg", "meshes/g.stl").has_value());
}

TEST_CASE("a deeply nested package whose folder differs from its manifest name resolves", "[ros]")
{
    temp_tree tree;
    write_file(tree.root / "a" / "b" / "arm_description_dir" / "package.xml",
        "<package><name>arm_description</name></package>");
    write_file(tree.root / "a" / "b" / "arm_description_dir" / "meshes" / "x.stl", "solid\n");

    std::vector<std::pair<meios::level, std::string>> log_entries;
    meios::log_sink_f log{ capture{ log_entries } };
    meios::ros_package_source source({ tree.root }, {}, log);

    REQUIRE(has_package(source.packages(), "arm_description"));
    REQUIRE(source.locate("arm_description", "meshes/x.stl").has_value());
    REQUIRE(source.path_of("arm_description", "").has_value());
}

TEST_CASE("stack order encodes explicit-first precedence and shadows lower layers", "[ros]")
{
    temp_tree explicit_tree;
    temp_tree ament_tree;
    write_file(explicit_tree.root / "arm" / "meshes" / "x.stl", "explicit\n");
    make_ament_package(ament_tree.root, "arm");

    std::vector<std::pair<meios::level, std::string>> log_entries;
    meios::log_sink_f log{ capture{ log_entries } };
    meios::log_sink &seam = log;

    meios::source_stack stack{
        meios::directory_source{ explicit_tree.root, seam },
        meios::ros_package_source{ {}, { ament_tree.root }, seam },
    };

    std::optional<meios::resolved_asset> hit = stack.locate("arm", "meshes/x.stl", seam);
    REQUIRE(hit.has_value());
    std::ifstream resolved(hit->path());
    const std::string content((std::istreambuf_iterator<char>(resolved)),
                              std::istreambuf_iterator<char>());
    REQUIRE(content == "explicit\n");

    const bool shadowed = std::any_of(log_entries.begin(), log_entries.end(),
        [](const auto &e) { return e.first == meios::level::info; });
    REQUIRE(shadowed);
}

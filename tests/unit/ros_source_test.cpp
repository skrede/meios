#include "ros_package_fixture.h"

#include <meios/ros/ros_package_source.h>

#include <meios/io/source_stack.h>
#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <optional>
#include <algorithm>
#include <filesystem>

static_assert(meios::package_source<meios::ros_package_source>);
static_assert(meios::provides_path<meios::ros_package_source>);
static_assert(meios::enumerates_packages<meios::ros_package_source>);

namespace
{
using acquisition_test::make_tree;
using acquisition_test::record;
using acquisition_test::recorder;
using ros_test::ament_package;
using ros_test::has_package;
using ros_test::mesh;
using ros_test::require_authorized;
using ros_test::ros1_package;
using ros_test::write_file;
}

TEST_CASE("a ROS2 ament prefix resolves a share file and authorizes it", "[ros]")
{
    const meios::scratch_dir tree    = make_tree();
    const std::filesystem::path base = ament_package(tree.path(), "arm");
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::ros_package_source source({}, {tree.path()}, log);

    REQUIRE(source.capabilities().kind == meios::source_kind::directory);
    REQUIRE(source.capabilities().package_enumerable);
    REQUIRE(has_package(source.packages(), "arm"));
    require_authorized(source.locate("arm", mesh), base);
}

TEST_CASE("a relative that escapes the resolved share dir is rejected", "[ros]")
{
    const meios::scratch_dir tree = make_tree();
    ament_package(tree.path(), "arm");
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::ros_package_source source({}, {tree.path()}, log);
    const auto refused = [](const record &noted) { return noted.level == meios::level::error && noted.code == meios::diagnostic_code::uncontained_asset; };

    REQUIRE_FALSE(source.locate("arm", "../../../etc/passwd").has_value());
    REQUIRE(std::any_of(records.begin(), records.end(), refused));
}

TEST_CASE("a ROS1 crawl names packages from the manifest and authorizes the hit", "[ros]")
{
    const meios::scratch_dir tree      = make_tree();
    const std::filesystem::path nested = ros1_package(tree.path() / "a" / "b" / "arm_description_dir", "arm_description");
    write_file(tree.path() / "hidden" / "package.xml", "<package><name>hidden_pkg</name></package>");
    write_file(tree.path() / "hidden" / "CATKIN_IGNORE", "");
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::ros_package_source source({tree.path()}, {}, log);

    const std::vector<std::string> names = source.packages();
    REQUIRE(has_package(names, "arm_description"));
    REQUIRE_FALSE(has_package(names, "hidden_pkg"));
    REQUIRE(source.path_of("arm_description", "").has_value());
    require_authorized(source.locate("arm_description", mesh), nested);
}

TEST_CASE("a colcon --symlink-install package dir resolves to the real files", "[ros]")
{
    const meios::scratch_dir tree       = make_tree();
    const std::filesystem::path outside = tree.path() / "src" / "arm_pkg";
    const std::filesystem::path real    = ros1_package(outside, "arm_hardware");
    const std::filesystem::path ws      = tree.path() / "install";
    std::filesystem::create_directories(ws);
    std::error_code ec;
    std::filesystem::create_directory_symlink(outside, ws / "arm_hardware", ec);
    REQUIRE_FALSE(ec);

    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::ros_package_source source({ws}, {}, log);

    REQUIRE(has_package(source.packages(), "arm_hardware"));
    require_authorized(source.locate("arm_hardware", mesh), real);
    REQUIRE(std::filesystem::equivalent(source.locate("arm_hardware", mesh)->path(), outside / mesh));
}

TEST_CASE("an escaping in-root symlink with no manifest is never registered", "[ros]")
{
    const meios::scratch_dir tree       = make_tree();
    const std::filesystem::path outside = tree.path() / "secret";
    write_file(outside / "passwd", "secret-bytes");
    const std::filesystem::path ws = tree.path() / "ws";
    std::filesystem::create_directories(ws);
    std::error_code ec;
    std::filesystem::create_directory_symlink(outside, ws / "evil", ec);
    REQUIRE_FALSE(ec);

    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::ros_package_source source({ws}, {}, log);

    REQUIRE_FALSE(has_package(source.packages(), "evil"));
    REQUIRE_FALSE(source.locate("evil", "passwd").has_value());
}

TEST_CASE("stack order encodes explicit-first precedence and shadows lower layers", "[ros]")
{
    const meios::scratch_dir explicit_tree = make_tree();
    const meios::scratch_dir ament_tree    = make_tree();
    write_file(explicit_tree.path() / "arm" / mesh, "explicit\n");
    ament_package(ament_tree.path(), "arm");

    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::log_sink &seam = log;
    meios::source_stack stack{meios::directory_source{explicit_tree.path(), seam}, meios::ros_package_source{{}, {ament_tree.path()}, seam}};

    const std::optional<meios::resolved_asset> hit = stack.locate("arm", mesh, seam);
    REQUIRE(hit.has_value());
    std::ifstream resolved(hit->path());
    const std::string content((std::istreambuf_iterator<char>(resolved)), std::istreambuf_iterator<char>());
    REQUIRE(content == "explicit\n");
    REQUIRE(std::any_of(records.begin(), records.end(), [](const record &noted) { return noted.level == meios::level::info; }));
}

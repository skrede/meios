#include "source_lookup_fixture.h"

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
#include <string_view>

static_assert(meios::package_source<meios::ros_package_source>);
static_assert(meios::provides_path<meios::ros_package_source>);
static_assert(meios::enumerates_packages<meios::ros_package_source>);

namespace
{
using acquisition_test::make_tree;
using acquisition_test::real_path;
using acquisition_test::record;
using acquisition_test::recorder;

constexpr std::string_view mesh = "meshes/x.stl";

void write_file(const std::filesystem::path &path, std::string_view content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

// Both seeders answer with the base the discovery layer will register for the package, so a
// case compares authorization against the layout it wrote rather than against a returned path.
std::filesystem::path ament_package(const std::filesystem::path &prefix, std::string_view pkg)
{
    write_file(prefix / "share" / "ament_index" / "resource_index" / "packages" / pkg, "");
    write_file(prefix / "share" / pkg / mesh, "solid\n");
    return meios::detail::absolute_base(prefix) / "share" / pkg;
}

std::filesystem::path ros1_package(const std::filesystem::path &dir, std::string_view name)
{
    write_file(dir / "package.xml", "<package><name>" + std::string(name) + "</name></package>");
    write_file(dir / mesh, "solid\n");
    return real_path(dir);
}

bool has_package(const std::vector<std::string> &names, std::string_view wanted)
{
    return std::find(names.begin(), names.end(), std::string(wanted)) != names.end();
}

// A hit authorizes a later checked open with the base the discovery layer found and the
// caller's own relative half, neither of them recovered from the presentation path.
void require_authorized(const std::optional<meios::resolved_asset> &hit, const std::filesystem::path &base)
{
    REQUIRE(hit.has_value());
    REQUIRE(hit->path().is_absolute());
    REQUIRE(std::filesystem::exists(hit->path()));
    REQUIRE(hit->source_root() == std::optional{base});
    REQUIRE(hit->source_relative() == std::optional{std::filesystem::path(mesh)});
    REQUIRE(real_path(base / mesh) == real_path(hit->path()));
}

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

TEST_CASE("a relative ament prefix resolves and authorizes against the same base", "[ros]")
{
    const meios::scratch_dir tree = make_tree();
    ament_package(tree.path(), "arm");
    const std::filesystem::path saved = std::filesystem::current_path();
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};

    std::filesystem::current_path(tree.path().parent_path());
    const std::filesystem::path base = meios::detail::absolute_base(tree.path().filename()) / "share" / "arm";
    meios::ros_package_source source({}, {tree.path().filename()}, log);
    const bool known                               = has_package(source.packages(), "arm");
    const std::optional<meios::resolved_asset> hit = source.locate("arm", mesh);
    std::filesystem::current_path(saved);

    REQUIRE(known);
    require_authorized(hit, base);
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

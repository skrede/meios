#include "ros_package_fixture.h"

#include <meios/ros/ros_package_source.h>

#include <meios/io/resolved_asset.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <cstdlib>
#include <optional>
#include <filesystem>

namespace
{
using acquisition_test::make_tree;
using acquisition_test::record;
using acquisition_test::recorder;
using ros_test::ament_package;
using ros_test::has_package;
using ros_test::mesh;
using ros_test::require_authorized;

void portable_setenv(const char *name, const char *value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

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

// The index is seeded exactly where an unguarded reader would find it: an empty prefix joined
// with "share" is a relative path, and a relative index path is listed against the process
// working directory rather than against a root the caller named.
TEST_CASE("an empty ament prefix registers nothing from the working directory", "[ros]")
{
    const meios::scratch_dir tree = make_tree();
    ament_package(tree.path(), "arm");
    const std::filesystem::path saved = std::filesystem::current_path();
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};

    std::filesystem::current_path(tree.path());
    meios::ros_package_source source({}, {std::filesystem::path{}}, log);
    const bool known   = has_package(source.packages(), "arm");
    const bool located = source.locate("arm", mesh).has_value();
    std::filesystem::current_path(saved);

    REQUIRE_FALSE(known);
    REQUIRE_FALSE(located);
}

TEST_CASE("an empty ament prefix environment value registers nothing", "[ros]")
{
    const meios::scratch_dir tree = make_tree();
    ament_package(tree.path(), "arm");
    const std::filesystem::path saved = std::filesystem::current_path();
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};

    portable_setenv("ROS_PACKAGE_PATH", "");
    portable_setenv("AMENT_PREFIX_PATH", "");
    std::filesystem::current_path(tree.path());
    meios::ros_package_source source = meios::ros_package_source::from_environment(log);
    const bool known                 = has_package(source.packages(), "arm");
    const bool located               = source.locate("arm", mesh).has_value();
    std::filesystem::current_path(saved);

    REQUIRE_FALSE(known);
    REQUIRE_FALSE(located);
}

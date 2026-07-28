#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <ostream>
#include <sstream>
#include <optional>
#include <iostream>
#include <filesystem>
#include <string_view>

namespace
{

struct captured
{
    std::vector<std::string> &msgs;

    void operator()(meios::level, const std::string &m) { msgs.push_back(m); }

    void operator()(meios::level, const meios::source_location &, const std::string &m)
    {
        msgs.push_back(m);
    }
};

std::filesystem::path fixture(std::string_view name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

std::filesystem::path make_pkg_root()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("meios_pkg_" + std::to_string(stamp));
    std::filesystem::create_directories(root / "somepkg" / "meshes");
    std::ofstream(root / "somepkg" / "meshes" / "x.stl") << "solid x\nendsolid x\n";
    return root;
}

std::optional<std::string> first_mesh_path(const meios::model<double> &robot)
{
    for(const meios::link<double> &node : robot.links)
        for(const meios::visual<double> &vis : node.visuals)
            if(const auto *shape = std::get_if<meios::mesh<double>>(&vis.geom.shape))
                return shape->resolved_path;
    return std::nullopt;
}

bool has_message(const std::vector<std::string> &msgs, std::string_view needle)
{
    for(const std::string &m : msgs)
        if(m.find(needle) != std::string::npos)
            return true;
    return false;
}

}

TEST_CASE("package_roots resolve a real package:// mesh end-to-end", "[urdf][load][sources]")
{
    const std::filesystem::path root = make_pkg_root();
    std::vector<std::string> msgs;
    meios::log_sink_f log{ captured{ msgs } };

    meios::load_options opts;
    opts.package_roots = { root };
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(fixture("package_mesh.urdf"), opts, log);
    REQUIRE(loaded.has_value());
    const meios::model<double> &robot = loaded->robot;

    const std::optional<std::string> resolved = first_mesh_path(robot);
    REQUIRE(resolved.has_value());
    REQUIRE_FALSE(resolved->empty());

    std::filesystem::remove_all(root);
}

TEST_CASE("a package miss is reported, not silently dropped", "[urdf][load][sources]")
{
    std::vector<std::string> msgs;
    meios::log_sink_f log{ captured{ msgs } };

    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(fixture("package_mesh.urdf"), opts, log);
    REQUIRE(loaded.has_value());
    const meios::model<double> &robot = loaded->robot;

    REQUIRE(has_message(msgs, "somepkg"));
    REQUIRE_FALSE(first_mesh_path(robot).value_or("").size() > 0);
}

TEST_CASE("the two-arg load stays silent while refusing an unresolved package",
          "[urdf][load][sources]")
{
    std::ostringstream redirect;
    std::streambuf *previous = std::cerr.rdbuf(redirect.rdbuf());

    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(fixture("package_mesh.urdf"), opts);

    std::cerr.rdbuf(previous);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error().code == meios::diagnostic_code::unresolved_mesh);
    REQUIRE(redirect.str().empty());
}

TEST_CASE("the source_stack overload threads a caller stack into resolution", "[urdf][load][sources]")
{
    const std::filesystem::path root = make_pkg_root();
    std::vector<std::string> msgs;
    meios::log_sink_f log{ captured{ msgs } };

    meios::capturing_log_sink capture{ log };
    meios::source_stack sources;
    sources.push_back(meios::source_handle(meios::directory_source(root, capture)));

    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(fixture("package_mesh.urdf"), opts, sources, capture);
    REQUIRE(loaded.has_value());
    const meios::model<double> &robot = loaded->robot;

    REQUIRE(first_mesh_path(robot).value_or("").size() > 0);

    std::filesystem::remove_all(root);
}

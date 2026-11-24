#include "urdf_mesh_fixture.h"

#include <meios/io.h>
#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <utility>
#include <variant>
#include <optional>
#include <filesystem>

namespace
{

const meios::mesh<double> &first_mesh(const meios::model<double> &robot)
{
    return std::get<meios::mesh<double>>(robot.links.at(0).visuals.at(0).geom.shape);
}

}

TEST_CASE("a package:// mesh resolves to a path through the source stack", "[urdf][mesh]")
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "meios_urdf_mesh_fixture";
    std::filesystem::create_directories(root / "somepkg" / "meshes");
    std::ofstream(root / "somepkg" / "meshes" / "x.stl") << "solid\n";

    meios::log_sink log;
    meios::directory_source dir{root, log};
    meios::source_stack sources{dir};
    const meios::tree<double> robot = parse_document("package_mesh.urdf", sources, meios::missing_asset::warn, log);

    REQUIRE(first_mesh(robot).resolved_path.has_value());
    REQUIRE(first_mesh(robot).resolved_path->find("x.stl") != std::string::npos);
    std::filesystem::remove_all(root);
}

TEST_CASE("a byte-backed mesh names a file that outlives the returned and moved result", "[urdf][mesh]")
{
    meios::log_sink inner;
    meios::capturing_log_sink log{inner};
    meios::memory_source memory{log};
    memory.add("somepkg", "meshes/x.stl", "solid\n");
    meios::source_stack sources{std::move(memory)};

    std::optional<meios::load_result> moved;
    {
        const meios::load_options opts;
        meios::expected<meios::load_result, meios::load_error> loaded = meios::load(mesh_fixture(), opts, sources, log);
        REQUIRE(loaded.has_value());
        moved = std::move(*loaded);
    }

    const meios::mesh<double> &shape = first_mesh(moved->robot);
    REQUIRE(shape.resolved_path.has_value());
    REQUIRE(std::filesystem::exists(*shape.resolved_path));
    REQUIRE(std::filesystem::path(*shape.resolved_path).extension() == ".stl");
}

TEST_CASE("the resolved path is gone once the stack that owned it is destroyed", "[urdf][mesh]")
{
    meios::log_sink inner;
    meios::capturing_log_sink log{inner};
    std::filesystem::path recorded;
    {
        meios::memory_source memory{log};
        memory.add("somepkg", "meshes/x.stl", "solid\n");
        meios::source_stack sources{std::move(memory)};

        const meios::load_options opts;
        meios::expected<meios::load_result, meios::load_error> loaded = meios::load(mesh_fixture(), opts, sources, log);
        REQUIRE(loaded.has_value());
        recorded = *first_mesh(loaded->robot).resolved_path;
        REQUIRE(std::filesystem::exists(recorded));
    }
    REQUIRE_FALSE(std::filesystem::exists(recorded));
}

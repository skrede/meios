#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <variant>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace
{

std::string slurp(const std::string &name)
{
    const std::filesystem::path path = std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

struct recorder
{
    std::vector<meios::level> &levels;

    void operator()(meios::level lvl, const std::string &) { levels.push_back(lvl); }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &)
    {
        levels.push_back(lvl);
    }
};

meios::tree<double> parse_mesh(meios::source_stack &sources, meios::missing_asset miss,
                               meios::log_sink &log)
{
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources, eval, log, miss, meios::topology_policy::fail,
                              meios::material_policy::warn, meios::strictness::strict, {} };
    meios::pod_recorder<meios::tree<double>> rec(log, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp("package_mesh.urdf"), rec);
    return rec.result();
}

const meios::mesh<double> &first_mesh(const meios::tree<double> &robot)
{
    return std::get<meios::mesh<double>>(robot.links.at(0).visuals.at(0).geom.shape);
}

}

TEST_CASE("a package:// mesh resolves to a path through the source stack", "[urdf][mesh]")
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "meios_urdf_mesh_fixture";
    std::filesystem::create_directories(root / "somepkg" / "meshes");
    std::ofstream(root / "somepkg" / "meshes" / "x.stl") << "solid\n";

    meios::log_sink log;
    meios::directory_source dir{ root, log };
    meios::source_stack sources{ dir };
    const meios::tree<double> robot = parse_mesh(sources, meios::missing_asset::warn, log);

    REQUIRE(first_mesh(robot).resolved_path.has_value());
    REQUIRE(first_mesh(robot).resolved_path->find("x.stl") != std::string::npos);
    std::filesystem::remove_all(root);
}

TEST_CASE("a bytes-backed package mesh materializes to a temp path", "[urdf][mesh]")
{
    meios::log_sink log;
    meios::memory_source memory;
    memory.add("somepkg", "meshes/x.stl", "solid\n");
    meios::source_stack sources{ memory };
    const meios::tree<double> robot = parse_mesh(sources, meios::missing_asset::warn, log);

    REQUIRE(first_mesh(robot).resolved_path.has_value());
    REQUIRE_FALSE(first_mesh(robot).resolved_path->empty());
}

TEST_CASE("a resolved mesh that is an unsmudged Git-LFS pointer is rejected, not accepted",
          "[urdf][mesh]")
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "meios_urdf_lfs_fixture";
    std::filesystem::create_directories(root / "somepkg" / "meshes");
    std::ofstream(root / "somepkg" / "meshes" / "x.stl")
        << "version https://git-lfs.github.com/spec/v1\n"
           "oid sha256:4d7a2146e8f0a9c1b2d3e4f5061728394a5b6c7d8e9f0a1b2c3d4e5f60718293\n"
           "size 12345\n";

    std::vector<meios::level> levels;
    meios::log_sink_f capture{ recorder{ levels } };
    meios::directory_source dir{ root, capture };
    meios::source_stack sources{ dir };
    const meios::tree<double> robot = parse_mesh(sources, meios::missing_asset::warn, capture);

    REQUIRE_FALSE(first_mesh(robot).resolved_path.has_value());
    REQUIRE(std::count(levels.begin(), levels.end(), meios::level::error) >= 1);
    std::filesystem::remove_all(root);
}

TEST_CASE("a missing mesh honors the missing_asset policy", "[urdf][mesh]")
{
    SECTION("fail logs an error and leaves the path empty")
    {
        std::vector<meios::level> levels;
        meios::log_sink_f capture{ recorder{ levels } };
        meios::source_stack sources{};
        const meios::tree<double> robot = parse_mesh(sources, meios::missing_asset::fail, capture);

        REQUIRE_FALSE(first_mesh(robot).resolved_path.has_value());
        REQUIRE(std::count(levels.begin(), levels.end(), meios::level::error) >= 1);
    }

    SECTION("warn logs a warning and leaves the path empty")
    {
        std::vector<meios::level> levels;
        meios::log_sink_f capture{ recorder{ levels } };
        meios::source_stack sources{};
        const meios::tree<double> robot = parse_mesh(sources, meios::missing_asset::warn, capture);

        REQUIRE_FALSE(first_mesh(robot).resolved_path.has_value());
        REQUIRE(std::count(levels.begin(), levels.end(), meios::level::warn) >= 1);
    }

    SECTION("skip stays silent and leaves the path empty")
    {
        std::vector<meios::level> levels;
        meios::log_sink_f capture{ recorder{ levels } };
        meios::source_stack sources{};
        const meios::tree<double> robot = parse_mesh(sources, meios::missing_asset::skip, capture);

        REQUIRE_FALSE(first_mesh(robot).resolved_path.has_value());
        REQUIRE(levels.empty());
    }
}

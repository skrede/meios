#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/bundle/flatten.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <random>
#include <cstdlib>
#include <variant>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <system_error>

#ifndef MEIOS_FLATTEN_GOLDEN_DIR
    #error "MEIOS_FLATTEN_GOLDEN_DIR must name the flatten baseline directory"
#endif
#ifndef MEIOS_URDF_FIXTURE_DIR
    #error "MEIOS_URDF_FIXTURE_DIR must name the urdf fixture directory"
#endif

namespace
{

std::string slurp(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void seed(const std::filesystem::path &file)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << "seed\n";
}

// A leak of a resolved path can only be seen where resolution happened, so the document is
// driven from a tree built here: a package root holding the mesh, and the texture beside the
// document the relative form is measured against.
class sandbox
{
public:
    sandbox() : m_root(std::filesystem::temp_directory_path() / stem())
    {
        seed(m_root / "somepkg" / "meshes" / "base.stl");
        seed(m_root / "textures" / "skin.png");
        std::filesystem::copy_file(
            std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "flatten" / "resolved_assets.urdf",
            document());
    }

    sandbox(const sandbox &) = delete;

    sandbox &operator=(const sandbox &) = delete;

    ~sandbox()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_root, ec);
    }

    const std::filesystem::path &root() const { return m_root; }

    std::filesystem::path document() const { return m_root / "resolved_assets.urdf"; }

private:
    std::filesystem::path m_root;

    static std::string stem()
    {
        std::random_device entropy;
        std::ostringstream out;
        out << "meios-flatten-" << std::hex << entropy() << entropy();
        return out.str();
    }
};

const meios::mesh<double> &only_mesh(const meios::model<double> &robot)
{
    REQUIRE(robot.links.size() == 1);
    REQUIRE(robot.links.front().visuals.size() == 1);
    const meios::mesh<double> *shape =
        std::get_if<meios::mesh<double>>(&robot.links.front().visuals.front().geom.shape);
    REQUIRE(shape != nullptr);
    return *shape;
}

const meios::material<double> &only_material(const meios::model<double> &robot)
{
    REQUIRE(robot.materials.size() == 1);
    return robot.materials.front();
}

std::string flattened(const meios::model<double> &robot, meios::log_sink &log)
{
    std::ostringstream out;
    const meios::emit_result result = meios::flatten(robot, out, log);
    REQUIRE(result.status == meios::emit_status::ok);
    return out.str();
}

void compare_baseline(const std::string &produced)
{
    const std::filesystem::path baseline =
        std::filesystem::path{ MEIOS_FLATTEN_GOLDEN_DIR } / "resolved_assets.flat.urdf";
    // Regeneration hatch, matching the sibling instrument: MEIOS_FLATTEN_BLESS=1 rewrites
    // the baseline from the current output, so a deliberate change to it has one documented
    // command instead of a hand-edited golden. Off in every normal run.
    if(const char *bless = std::getenv("MEIOS_FLATTEN_BLESS"); bless != nullptr && bless[0] == '1')
        std::ofstream(baseline, std::ios::binary | std::ios::trunc) << produced;
    INFO("baseline: " << baseline.string());
    REQUIRE(std::filesystem::exists(baseline));
    REQUIRE(produced == slurp(baseline));
}

}

TEST_CASE("a flattened document carries the authored URI for a mesh and a texture that resolved",
          "[urdf][flatten][asset]")
{
    const sandbox tree;

    meios::log_sink discard;
    meios::load_options opts;
    opts.package_roots = { tree.root() };
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(tree.document(), opts, discard);
    REQUIRE(loaded.has_value());

    const meios::mesh<double> &shape = only_mesh(loaded->robot);
    const meios::material<double> &paint = only_material(loaded->robot);
    REQUIRE(shape.resolved_path.has_value());
    REQUIRE(*shape.resolved_path != shape.filename);
    REQUIRE(paint.resolved_texture.has_value());
    REQUIRE(*paint.resolved_texture != *paint.texture);

    const std::string produced = flattened(loaded->robot, discard);
    REQUIRE(produced.find(shape.filename) != std::string::npos);
    REQUIRE(produced.find(*paint.texture) != std::string::npos);
    REQUIRE(produced.find(*shape.resolved_path) == std::string::npos);
    REQUIRE(produced.find(*paint.resolved_texture) == std::string::npos);

    compare_baseline(produced);
}
